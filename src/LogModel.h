#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

// Table model backing the log view. Each row is one parsed log line.
//
// D1-D4 loads synchronously; W/D6 will move parsing to a worker thread and
// stream rows in via beginInsertRows so the UI stays responsive on huge files.
class LogModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum class Level { Trace, Debug, Info, Warn, Error, Unknown };
    enum Column { Col_Line = 0, Col_Level, Col_Message, ColumnCount };

    explicit LogModel(QObject* parent = nullptr);

    // Reads the whole file and rebuilds the model. Returns false on failure and
    // fills *error (if provided) with a message.
    bool loadFile(const QString& path, QString* error = nullptr);
    void clear();

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    static Level detectLevel(const QString& line);
    static QString levelName(Level level);

private:
    struct Entry {
        int lineNo;
        Level level;
        QString text;
    };
    QVector<Entry> m_entries;
};
