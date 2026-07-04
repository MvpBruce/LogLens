#include "MainWindow.h"

#include "LogDelegate.h"
#include "LogFilterProxy.h"
#include "LogLoader.h"
#include "LogModel.h"

#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QSettings>
#include <QStatusBar>
#include <QTableView>
#include <QThread>
#include <QTimer>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_model(new LogModel(this)),
      m_proxy(new LogFilterProxy(this)),
      m_view(new QTableView(this)) {
    setWindowTitle(QStringLiteral("LogLens"));
    resize(1000, 640);
    setAcceptDrops(true);

    // View -> proxy (filter) -> model. The view never sees the model directly.
    m_proxy->setSourceModel(m_model);
    m_view->setModel(m_proxy);
    m_view->setItemDelegate(new LogDelegate(this)); // severity coloring
    m_view->setShowGrid(false);
    m_view->setAlternatingRowColors(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->setColumnWidth(LogModel::Col_Line, 70);
    m_view->setColumnWidth(LogModel::Col_Level, 70);
    setCentralWidget(m_view);

    buildFilterBar();

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* openAct = fileMenu->addAction(QStringLiteral("&Open..."));
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);
    fileMenu->addSeparator();
    QAction* quitAct = fileMenu->addAction(QStringLiteral("E&xit"));
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    // Progress bar lives at the right of the status bar, hidden when idle.
    m_progress = new QProgressBar(this);
    m_progress->setMaximumWidth(180);
    m_progress->setRange(0, 100);
    m_progress->hide();
    statusBar()->addPermanentWidget(m_progress);
    statusBar()->showMessage(QStringLiteral("Open a log file to begin"));

    startLoaderThread();
}

MainWindow::~MainWindow() {
    // Stop any in-flight parse and shut the worker thread down cleanly.
    if (m_loader)
        m_loader->cancel();
    if (m_thread) {
        m_thread->quit();
        m_thread->wait();
    }
}

void MainWindow::startLoaderThread() {
    m_thread = new QThread(this);
    m_loader = new LogLoader; // no parent: it is moved to the worker thread
    m_loader->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_loader, &QObject::deleteLater);

    // UI -> worker: queued because they live on different threads.
    connect(this, &MainWindow::requestLoad, m_loader, &LogLoader::load);

    // worker -> UI: each carries a generation so stale results are dropped.
    connect(m_loader, &LogLoader::batchReady, this,
            [this](quint64 gen, const QVector<LogModel::Entry>& batch) {
                if (gen == m_generation)
                    m_model->appendBatch(batch);
            });
    connect(m_loader, &LogLoader::progress, this, [this](quint64 gen, int pct) {
        if (gen == m_generation)
            m_progress->setValue(pct);
    });
    connect(m_loader, &LogLoader::finished, this,
            [this](quint64 gen, bool ok, const QString& error) {
                if (gen != m_generation)
                    return;
                m_progress->hide();
                if (!ok && error != QLatin1String("canceled")) {
                    QMessageBox::warning(this, QStringLiteral("LogLens"),
                                         QStringLiteral("Failed to load:\n%1")
                                             .arg(error));
                }
                updateStatus();
            });

    m_thread->start();
}

void MainWindow::onOpen() {
    QSettings settings;
    const QString lastDir =
        settings.value(QStringLiteral("lastDir")).toString();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open log file"), lastDir,
        QStringLiteral("Log files (*.log *.txt);;All files (*)"));
    if (path.isEmpty())
        return;
    settings.setValue(QStringLiteral("lastDir"), QFileInfo(path).absolutePath());
    openPath(path);
}

void MainWindow::openPath(const QString& path) {
    // Bump the generation so any in-flight load's remaining signals are ignored,
    // and cancel it so the worker stops reading promptly.
    ++m_generation;
    m_loader->cancel();

    m_model->beginStreaming(); // clear to empty; batches stream in
    m_progress->setValue(0);
    m_progress->show();
    setWindowTitle(QStringLiteral("LogLens — %1").arg(QFileInfo(path).fileName()));
    statusBar()->showMessage(QStringLiteral("Loading %1...").arg(path));

    emit requestLoad(path, m_generation);
}

void MainWindow::buildFilterBar() {
    QToolBar* bar = addToolBar(QStringLiteral("Filter"));
    bar->setMovable(false);

    // Applying a filter re-scans the whole model on the UI thread, so we run it
    // at most once per action (never per-keystroke, never per-removed-row).
    // Re-filtering is a single call; updateStatus() is called once afterwards.
    auto applyQuery = [this] {
        m_proxy->setQuery(m_query->text());
        updateStatus();
    };

    m_filterTimer = new QTimer(this);
    m_filterTimer->setSingleShot(true);
    m_filterTimer->setInterval(150); // debounce typing
    connect(m_filterTimer, &QTimer::timeout, this, applyQuery);

    m_query = new QLineEdit(bar);
    m_query->setPlaceholderText(QStringLiteral("Filter messages..."));
    m_query->setClearButtonEnabled(true);
    m_query->setMinimumWidth(240);
    // Debounce: restart the timer on each keystroke; filter only once it settles.
    connect(m_query, &QLineEdit::textChanged, this,
            [this] { m_filterTimer->start(); });
    bar->addWidget(m_query);

    auto* regex = new QCheckBox(QStringLiteral("Regex"), bar);
    connect(regex, &QCheckBox::toggled, this, [this, applyQuery](bool on) {
        m_proxy->setUseRegex(on);
        applyQuery();
    });
    bar->addWidget(regex);

    bar->addSeparator();
    bar->addWidget(new QLabel(QStringLiteral(" Levels: "), bar));

    // One checkbox per severity; all start checked (visible).
    const struct {
        const char* text;
        LogModel::Level level;
    } levels[] = {
        {"Error", LogModel::Level::Error}, {"Warn", LogModel::Level::Warn},
        {"Info", LogModel::Level::Info},   {"Debug", LogModel::Level::Debug},
        {"Trace", LogModel::Level::Trace},
    };
    for (const auto& l : levels) {
        auto* cb = new QCheckBox(QString::fromLatin1(l.text), bar);
        cb->setChecked(true);
        const LogModel::Level level = l.level;
        connect(cb, &QCheckBox::toggled, this, [this, level](bool on) {
            m_proxy->setLevelEnabled(level, on); // one re-filter
            updateStatus();                      // one status update
        });
        bar->addWidget(cb);
    }
}

void MainWindow::updateStatus() {
    const int total = m_model->rowCount();
    const int shown = m_proxy->rowCount();
    if (total == 0) {
        statusBar()->showMessage(QStringLiteral("Open a log file to begin"));
        return;
    }
    if (shown == total)
        statusBar()->showMessage(QStringLiteral("%1 lines").arg(total));
    else
        statusBar()->showMessage(
            QStringLiteral("showing %1 of %2 lines").arg(shown).arg(total));
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty())
        openPath(urls.first().toLocalFile());
}
