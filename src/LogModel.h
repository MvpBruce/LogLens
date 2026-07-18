#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

class LogModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum class Level { Trace, Debug, Info, Warn, Error, Unknown };
    enum Column {
        Col_Line = 0,
        Col_Time,
        Col_Level,
        Col_Source,
        Col_Message,
        ColumnCount
    };

    // Custom roles expose parsed data without making callers re-parse text.
    enum Role {
        LevelRole = Qt::UserRole + 1,
        TimeRole,
        SourceRole,
        RawTextRole
    };

    static constexpr int LevelCount = 6; // keep in sync with enum Level

    // One parsed log line. Public so the background loader can build batches of
    // these off the UI thread and hand them back via a queued signal.
    struct Entry {
        int lineNo;
        Level level;
        QString time;
        QString source;
        QString message;
        QString rawText;
    };

    explicit LogModel(QObject* parent = nullptr);

    // Reads the whole file synchronously and rebuilds the model. Used by the
    // headless CLI; the GUI streams instead (see beginStreaming/appendBatch).
    bool loadFile(const QString& path, QString* error = nullptr);
    void clear();

    // Streaming API, driven by the background LogLoader:
    //   beginStreaming() resets to empty, then appendBatch() is called
    //   repeatedly on the UI thread as the worker parses the file.
    void beginStreaming();
    void appendBatch(const QVector<Entry>& batch);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    static Level detectLevel(const QString& line);
    static Entry parseLine(int lineNo, const QString& line);
    static QString levelName(Level level);

private:
    QVector<Entry> m_entries;
};

// Needed so batches can cross threads via a queued signal/slot connection.
Q_DECLARE_METATYPE(QVector<LogModel::Entry>)
