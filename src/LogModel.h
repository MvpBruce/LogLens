#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

class LogModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum class Level { Trace, Debug, Info, Warn, Error, Unknown };
    enum Column { Col_Line = 0, Col_Level, Col_Message, ColumnCount };

    // Custom role: exposes the raw Level (as int) so the filter proxy and the
    // coloring delegate can query severity without re-parsing the text.
    enum Role { LevelRole = Qt::UserRole + 1 };

    static constexpr int LevelCount = 6; // keep in sync with enum Level

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
