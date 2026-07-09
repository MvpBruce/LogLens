#include "LogModel.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>

LogModel::LogModel(QObject* parent) : QAbstractTableModel(parent) {}

bool LogModel::loadFile(const QString& path, QString* error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    beginResetModel();
    m_entries.clear();

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    int lineNo = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNo;
        m_entries.push_back({lineNo, detectLevel(line), line});
    }

    endResetModel();
    return true;
}

void LogModel::clear() {
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

void LogModel::beginStreaming() {
    // Discard any current content and prepare for incoming batches.
    clear();
}

void LogModel::appendBatch(const QVector<Entry>& batch) {
    if (batch.isEmpty())
        return;
    const int first = static_cast<int>(m_entries.size());
    beginInsertRows({}, first, first + static_cast<int>(batch.size()) - 1);
    m_entries += batch;
    endInsertRows();
}

int LogModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int LogModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant LogModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size())
        return {};

    const Entry& e = m_entries[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case Col_Line:
            return e.lineNo;
        case Col_Level:
            return levelName(e.level);
        case Col_Message:
            return e.text;
        }
    }

    // Raw severity for the filter proxy / coloring delegate (any column).
    if (role == LevelRole)
        return static_cast<int>(e.level);

    return {};
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation,
                              int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    switch (section) {
    case Col_Line:
        return QStringLiteral("#");
    case Col_Level:
        return QStringLiteral("Level");
    case Col_Message:
        return QStringLiteral("Message");
    }
    return {};
}

LogModel::Level LogModel::detectLevel(const QString& line) {
    // Word-boundary match so "INFORMATION" doesn't count as INFO, etc.
    static const QRegularExpression re(
        QStringLiteral(
            "\\b(ERROR|ERR|FATAL|WARN(?:ING)?|INFO|DEBUG|TRACE)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(line);
    if (!m.hasMatch())
        return Level::Unknown;

    const QString tok = m.captured(1).toUpper();
    if (tok == "ERROR" || tok == "ERR" || tok == "FATAL")
        return Level::Error;
    if (tok == "WARN" || tok == "WARNING")
        return Level::Warn;
    if (tok == "INFO")
        return Level::Info;
    if (tok == "DEBUG")
        return Level::Debug;
    if (tok == "TRACE")
        return Level::Trace;
    return Level::Unknown;
}

QString LogModel::levelName(Level level) {
    switch (level) {
    case Level::Trace:
        return QStringLiteral("TRACE");
    case Level::Debug:
        return QStringLiteral("DEBUG");
    case Level::Info:
        return QStringLiteral("INFO");
    case Level::Warn:
        return QStringLiteral("WARN");
    case Level::Error:
        return QStringLiteral("ERROR");
    case Level::Unknown:
        break;
    }
    return QStringLiteral("-");
}
