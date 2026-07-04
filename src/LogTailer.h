#pragma once

#include <QObject>
#include <QString>

class LogModel;
class QFileSystemWatcher;

// Watches the open file and streams appended lines into the model (tail -f).
//
// Runs on the UI thread: tail increments are tiny, so there is no need for a
// worker. Reads incrementally from a byte offset (never re-scanning the whole
// file) and in binary mode (so \r\n translation can't desync the offset).
class LogTailer : public QObject {
    Q_OBJECT
public:
    explicit LogTailer(LogModel* model, QObject* parent = nullptr);

    // Begin watching `path`, treating everything before `offset` as already
    // loaded. New bytes past `offset` are parsed and appended.
    void start(const QString& path, qint64 offset);
    void stop();
    bool isActive() const { return m_active; }

signals:
    void appended(); // emitted after new lines are added

private slots:
    void onFileChanged(const QString& path);

private:
    void readNew();

    LogModel* m_model;
    QFileSystemWatcher* m_watcher;
    QString m_path;
    qint64 m_offset = 0;
    bool m_active = false;
};
