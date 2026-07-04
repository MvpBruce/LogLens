#pragma once

#include <QMainWindow>

class LogModel;
class LogFilterProxy;
class LogLoader;
class LogTailer;
class QCheckBox;
class QLineEdit;
class QProgressBar;
class QTableView;
class QThread;
class QTimer;

// Main editor window: a filtered table view of the loaded log plus File > Open.
// File parsing runs on a background thread (LogLoader) and streams into the model.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Loads a file into the view (used by File > Open, drag-drop, and CLI arg).
    void openPath(const QString& path);

signals:
    // Handed to the worker thread's LogLoader::load via a queued connection.
    void requestLoad(const QString& path, quint64 generation);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onOpen();

private:
    void buildFilterBar();    // toolbar: text query, regex, level checkboxes, tail
    void startLoaderThread(); // spin up the worker + wire its signals
    void startTailing();      // follow the current file for appended lines
    void stopTailing();
    void updateStatus();      // "showing X of N lines"

    LogModel* m_model;
    LogFilterProxy* m_proxy;
    QTableView* m_view;
    QLineEdit* m_query = nullptr;
    QTimer* m_filterTimer = nullptr; // debounces text filtering

    QThread* m_thread = nullptr;
    LogLoader* m_loader = nullptr;
    QProgressBar* m_progress = nullptr;
    quint64 m_generation = 0; // bumped per load; stale signals are ignored

    LogTailer* m_tailer = nullptr;
    QCheckBox* m_tailToggle = nullptr;
    QString m_currentPath;
    bool m_tailing = false;      // auto-scroll only while actively tailing
    bool m_stickToBottom = false; // view was at the bottom before an append
};
