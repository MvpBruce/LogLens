#include "LogModel.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <QTextStream>

namespace {
LogModel::Level levelFromToken(QString token) {
    token = token.toUpper();
    if (token == "ERROR" || token == "ERR" || token == "FATAL")
        return LogModel::Level::Error;
    if (token == "WARN" || token == "WARNING")
        return LogModel::Level::Warn;
    if (token == "INFO")
        return LogModel::Level::Info;
    if (token == "DEBUG")
        return LogModel::Level::Debug;
    if (token == "TRACE")
        return LogModel::Level::Trace;
    return LogModel::Level::Unknown;
}
} // namespace

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
        m_entries.push_back(parseLine(lineNo, line));
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
        case Col_Time:
            return e.time.isEmpty() ? QStringLiteral("-") : e.time;
        case Col_Level:
            return levelName(e.level);
        case Col_Source:
            return e.source.isEmpty() ? QStringLiteral("-") : e.source;
        case Col_Message:
            return e.message;
        }
    }

    // Raw severity for the filter proxy / coloring delegate (any column).
    if (role == LevelRole)
        return static_cast<int>(e.level);
    if (role == TimeRole)
        return e.time;
    if (role == SourceRole)
        return e.source;
    if (role == RawTextRole)
        return e.rawText;

    return {};
}

QVariant LogModel::headerData(int section, Qt::Orientation orientation,
                              int role) const {
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return {};
    switch (section) {
    case Col_Line:
        return QStringLiteral("#");
    case Col_Time:
        return QStringLiteral("Time");
    case Col_Level:
        return QStringLiteral("Level");
    case Col_Source:
        return QStringLiteral("Source");
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

    return levelFromToken(m.captured(1));
}

LogModel::Entry LogModel::parseLine(int lineNo, const QString& line) {
    Entry entry{lineNo, detectLevel(line), QString(), QString(), line, line};
    QString rest = line;
    bool parsedPrefix = false;

    static const QRegularExpression timeRe(
        QStringLiteral(
            "^\\s*\\[?(\\d{4}-\\d{2}-\\d{2}[ T]\\d{2}:\\d{2}:\\d{2}"
            "(?:[\\.,]\\d+)?(?:Z|[+-]\\d{2}:?\\d{2})?)\\]?\\s*"));
    QRegularExpressionMatch timeMatch = timeRe.match(rest);
    if (timeMatch.hasMatch()) {
        entry.time = timeMatch.captured(1);
        rest = rest.mid(timeMatch.capturedEnd()).trimmed();
        parsedPrefix = true;
    }

    static const QRegularExpression levelRe(
        QStringLiteral(
            "^\\[?(ERROR|ERR|FATAL|WARN(?:ING)?|INFO|DEBUG|TRACE)\\]?\\s*"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch levelMatch = levelRe.match(rest);
    if (levelMatch.hasMatch()) {
        entry.level = levelFromToken(levelMatch.captured(1));
        rest = rest.mid(levelMatch.capturedEnd()).trimmed();
        parsedPrefix = true;
    }

    if (parsedPrefix) {
        static const QRegularExpression bracketSourceRe(
            QStringLiteral("^\\[([^\\]]+)\\]\\s*"));
        static const QRegularExpression parenSourceRe(
            QStringLiteral("^\\(([^\\)]+)\\)\\s*"));
        static const QRegularExpression tokenSourceRe(
            QStringLiteral("^([A-Za-z_][A-Za-z0-9_.-]*)\\s*(?::|-)\\s+"));

        QRegularExpressionMatch sourceMatch = bracketSourceRe.match(rest);
        if (!sourceMatch.hasMatch())
            sourceMatch = parenSourceRe.match(rest);
        if (!sourceMatch.hasMatch())
            sourceMatch = tokenSourceRe.match(rest);

        if (sourceMatch.hasMatch()) {
            entry.source = sourceMatch.captured(1);
            rest = rest.mid(sourceMatch.capturedEnd()).trimmed();
        }
    }

    entry.message = rest;
    return entry;
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
