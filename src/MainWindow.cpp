#include "MainWindow.h"

#include "LogModel.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QStatusBar>
#include <QTableView>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      m_model(new LogModel(this)),
      m_view(new QTableView(this)) {
    setWindowTitle(QStringLiteral("LogLens"));
    resize(1000, 640);
    setAcceptDrops(true);

    m_view->setModel(m_model);
    m_view->setShowGrid(false);
    m_view->setAlternatingRowColors(true);
    m_view->verticalHeader()->setVisible(false);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->horizontalHeader()->setStretchLastSection(true);
    m_view->setColumnWidth(LogModel::Col_Line, 70);
    m_view->setColumnWidth(LogModel::Col_Level, 70);
    setCentralWidget(m_view);

    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    QAction* openAct = fileMenu->addAction(QStringLiteral("&Open..."));
    openAct->setShortcut(QKeySequence::Open);
    connect(openAct, &QAction::triggered, this, &MainWindow::onOpen);
    fileMenu->addSeparator();
    QAction* quitAct = fileMenu->addAction(QStringLiteral("E&xit"));
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    statusBar()->showMessage(QStringLiteral("Open a log file to begin"));
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
    QString error;
    if (!m_model->loadFile(path, &error)) {
        QMessageBox::warning(this, QStringLiteral("LogLens"),
                             QStringLiteral("Failed to open %1:\n%2")
                                 .arg(path, error));
        return;
    }
    setWindowTitle(QStringLiteral("LogLens — %1").arg(QFileInfo(path).fileName()));
    statusBar()->showMessage(
        QStringLiteral("%1  (%2 lines)")
            .arg(path)
            .arg(m_model->rowCount()));
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
