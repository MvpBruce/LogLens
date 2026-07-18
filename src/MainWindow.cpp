#include "MainWindow.h"

#include "LogDelegate.h"
#include "LogFilterProxy.h"
#include "LogLoader.h"
#include "LogModel.h"
#include "LogTailer.h"

#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPalette>
#include <QProgressBar>
#include <QScrollBar>
#include <QSettings>
#include <QStatusBar>
#include <QTableView>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QToolBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_model(new LogModel(this)),
      m_proxy(new LogFilterProxy(this)),
      m_delegate(new LogDelegate(this)),
      m_view(new QTableView(this)) {
    setWindowTitle(QStringLiteral("LogLens"));
    resize(1000, 640);
    setAcceptDrops(true);

    // View -> proxy (filter) -> model. The view never sees the model directly.
    m_proxy->setSourceModel(m_model);
    m_view->setModel(m_proxy);
    m_view->setItemDelegate(m_delegate); // severity coloring + match highlights
    m_view->setShowGrid(false);
    m_view->setAlternatingRowColors(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->horizontalHeader()->setStretchLastSection(true);

    // Find keeps focus in the search box, so the table is "inactive" when a row
    // is selected. Make the inactive highlight match the active one so search
    // hits stay clearly visible.
    QPalette pal = m_view->palette();
    pal.setColor(QPalette::Inactive, QPalette::Highlight,
                 pal.color(QPalette::Active, QPalette::Highlight));
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText,
                 pal.color(QPalette::Active, QPalette::HighlightedText));
    m_view->setPalette(pal);
    m_view->setColumnWidth(LogModel::Col_Line, 70);
    m_view->setColumnWidth(LogModel::Col_Time, 180);
    m_view->setColumnWidth(LogModel::Col_Level, 70);
    m_view->setColumnWidth(LogModel::Col_Source, 140);
    setCentralWidget(m_view);

    // Live tail: parse appended lines and optionally keep the view pinned to
    // the bottom. The at-bottom check must be sampled before rows arrive.
    m_tailer = new LogTailer(m_model, this);
    connect(m_tailer, &LogTailer::appended, this, &MainWindow::updateStatus);
    connect(m_tailer, &LogTailer::truncated, this,
            [this](qint64 previousOffset, qint64 newSize) {
                statusBar()->showMessage(
                    QStringLiteral(
                        "File was truncated or rotated; resumed from byte 0 "
                        "(old offset %1, new size %2)")
                        .arg(previousOffset)
                        .arg(newSize));
            });
    connect(m_model, &QAbstractItemModel::rowsAboutToBeInserted, this, [this] {
        if (!m_tailing || !m_autoScroll)
            return;
        const QScrollBar* sb = m_view->verticalScrollBar();
        m_stickToBottom = sb->value() >= sb->maximum() - 2;
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this] {
        if (m_tailing && m_autoScroll && m_stickToBottom)
            m_view->scrollToBottom();
    });

    buildFilterBar();
    buildFindBar();

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* openAct = fileMenu->addAction(QStringLiteral("&Open..."));
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);
    QAction* exportAct =
        fileMenu->addAction(QStringLiteral("&Export Filtered..."));
    connect(exportAct, &QAction::triggered, this, &MainWindow::exportFiltered);
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
            [this](quint64 gen, bool ok, const QString& error, qint64 offset) {
                if (gen != m_generation)
                    return;
                m_loadedOffset = offset;
                m_progress->hide();
                if (!ok && error != QLatin1String("canceled")) {
                    QMessageBox::warning(this, QStringLiteral("LogLens"),
                                         QStringLiteral("Failed to load:\n%1")
                                             .arg(error));
                }
                updateStatus();
                // Resume following the file now that the initial load is done.
                if (ok && m_tailToggle && m_tailToggle->isChecked())
                    startTailing();
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

    // Pause tailing during load; it resumes on finish if "Tail -f" is checked.
    stopTailing();
    m_currentPath = path;
    m_loadedOffset = 0;

    m_model->beginStreaming(); // clear to empty; batches stream in
    m_progress->setValue(0);
    m_progress->show();
    setWindowTitle(QStringLiteral("LogLens - %1").arg(QFileInfo(path).fileName()));
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
        if (m_proxy->hasRegexError()) {
            m_delegate->setFilterHighlight(QString(), false);
            m_view->viewport()->update();
            statusBar()->showMessage(
                QStringLiteral("Invalid regex: %1")
                    .arg(m_proxy->regexErrorString()));
            return;
        }
        m_delegate->setFilterHighlight(m_query->text(),
                                       m_regexToggle && m_regexToggle->isChecked());
        m_view->viewport()->update();
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

    m_regexToggle = new QCheckBox(QStringLiteral("Regex"), bar);
    connect(m_regexToggle, &QCheckBox::toggled, this, [this, applyQuery](bool on) {
        m_proxy->setUseRegex(on);
        m_delegate->setFilterHighlight(m_query->text(), on);
        m_view->viewport()->update();
        applyQuery();
    });
    bar->addWidget(m_regexToggle);

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

    bar->addSeparator();
    m_tailToggle = new QCheckBox(QStringLiteral("Tail -f"), bar);
    connect(m_tailToggle, &QCheckBox::toggled, this, [this](bool on) {
        if (on)
            startTailing();
        else
            stopTailing();
    });
    bar->addWidget(m_tailToggle);

    m_autoScrollToggle = new QCheckBox(QStringLiteral("Auto-scroll"), bar);
    m_autoScrollToggle->setChecked(true);
    connect(m_autoScrollToggle, &QCheckBox::toggled, this, [this](bool on) {
        m_autoScroll = on;
        if (m_tailing && m_autoScroll)
            m_view->scrollToBottom();
    });
    bar->addWidget(m_autoScrollToggle);
}

void MainWindow::startTailing() {
    if (m_currentPath.isEmpty()) {
        m_tailing = false;
        return;
    }
    // Continue from the byte offset reached by the loader. This avoids losing
    // lines appended between initial load completion and watcher setup.
    m_tailer->start(m_currentPath, m_loadedOffset);
    m_tailing = true;
    if (m_autoScroll)
        m_view->scrollToBottom();
}

void MainWindow::stopTailing() {
    m_tailer->stop();
    m_tailing = false;
}

void MainWindow::buildFindBar() {
    addToolBarBreak(); // put Find on its own row below the filter bar
    QToolBar* bar = addToolBar(QStringLiteral("Find"));
    bar->setMovable(false);

    bar->addWidget(new QLabel(QStringLiteral(" Find: "), bar));
    m_find = new QLineEdit(bar);
    m_find->setPlaceholderText(QStringLiteral("Find in messages (Enter = next)"));
    m_find->setClearButtonEnabled(true);
    m_find->setMinimumWidth(240);
    connect(m_find, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_delegate->setFindHighlight(text);
        m_view->viewport()->update();
    });
    connect(m_find, &QLineEdit::returnPressed, this, [this] { findNext(true); });
    bar->addWidget(m_find);

    QAction* prev = bar->addAction(QStringLiteral("Prev"));
    prev->setShortcut(QKeySequence::FindPrevious); // Shift+F3
    connect(prev, &QAction::triggered, this, [this] { findNext(false); });

    QAction* next = bar->addAction(QStringLiteral("Next"));
    next->setShortcut(QKeySequence::FindNext); // F3
    connect(next, &QAction::triggered, this, [this] { findNext(true); });

    // Ctrl+F focuses the find box.
    QAction* focusFind = new QAction(this);
    focusFind->setShortcut(QKeySequence::Find);
    connect(focusFind, &QAction::triggered, this, [this] {
        m_find->setFocus();
        m_find->selectAll();
    });
    addAction(focusFind);
}

void MainWindow::findNext(bool forward) {
    const QString needle = m_find->text();
    const int rows = m_proxy->rowCount();
    if (needle.isEmpty() || rows == 0)
        return;

    // Search the (filtered) proxy rows, wrapping around, starting just past the
    // current selection in the chosen direction.
    const int step = forward ? 1 : -1;
    const int current = m_view->currentIndex().isValid()
                            ? m_view->currentIndex().row()
                            : (forward ? -1 : rows);
    for (int i = 1; i <= rows; ++i) {
        const int row = ((current + step * i) % rows + rows) % rows;
        const QString text =
            m_proxy->index(row, LogModel::Col_Message).data().toString();
        if (text.contains(needle, Qt::CaseInsensitive)) {
            const QModelIndex hit = m_proxy->index(row, LogModel::Col_Message);
            m_view->selectRow(row); // highlight the whole matching row
            m_view->setCurrentIndex(hit);
            m_view->scrollTo(hit, QAbstractItemView::PositionAtCenter);
            statusBar()->showMessage(
                QStringLiteral("Match at line %1")
                    .arg(m_proxy->index(row, LogModel::Col_Line).data().toInt()));
            return;
        }
    }
    statusBar()->showMessage(QStringLiteral("No match for \"%1\"").arg(needle));
}

void MainWindow::exportFiltered() {
    const int rows = m_proxy->rowCount();
    if (rows == 0) {
        statusBar()->showMessage(QStringLiteral("Nothing to export"));
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export filtered log"), QString(),
        QStringLiteral("Log files (*.log *.txt);;All files (*)"));
    if (path.isEmpty())
        return;

    QFile out(path);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("LogLens"),
                             QStringLiteral("Cannot write %1:\n%2")
                                 .arg(path, out.errorString()));
        return;
    }

    QTextStream ts(&out);
    for (int row = 0; row < rows; ++row)
        ts << m_proxy->index(row, LogModel::Col_Message)
                  .data(LogModel::RawTextRole)
                  .toString()
           << '\n';

    statusBar()->showMessage(
        QStringLiteral("Exported %1 lines to %2").arg(rows).arg(path));
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
