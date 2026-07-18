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
    void buildFindBar();      // second toolbar row: find text, prev/next
    void startLoaderThread(); // spin up the worker + wire its signals
    void startTailing();      // follow the current file for appended lines
    void stopTailing();
    void findNext(bool forward); // jump to the next/prev row matching the find box
    void exportFiltered();       // write the currently visible rows to a file
    void updateStatus();         // "showing X of N lines"

    LogModel* m_model;
    LogFilterProxy* m_proxy;
    QTableView* m_view;
    QLineEdit* m_query = nullptr;
    QTimer* m_filterTimer = nullptr; // debounces text filtering

    QThread* m_thread = nullptr;
    LogLoader* m_loader = nullptr;
    QProgressBar* m_progress = nullptr;
    quint64 m_generation = 0; // bumped per load; stale signals are ignored
    qint64 m_loadedOffset = 0; // byte offset reached by the current full load

    LogTailer* m_tailer = nullptr;
    QCheckBox* m_tailToggle = nullptr;
    QCheckBox* m_autoScrollToggle = nullptr;
    QLineEdit* m_find = nullptr;
    QString m_currentPath;
    bool m_tailing = false;      // true while actively reading appended lines
    bool m_autoScroll = true;    // true when new rows should keep the view pinned
    bool m_stickToBottom = false; // view was at the bottom before an append
};
