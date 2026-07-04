#include "LogFilterProxy.h"

LogFilterProxy::LogFilterProxy(QObject* parent) : QAbstractProxyModel(parent) {
    for (bool& enabled : m_levelEnabled)
        enabled = true; // all levels visible by default
}

void LogFilterProxy::setSourceModel(QAbstractItemModel* source) {
    if (sourceModel())
        disconnect(sourceModel(), nullptr, this, nullptr);

    QAbstractProxyModel::setSourceModel(source);

    if (source) {
        // beginStreaming()/loadFile() reset the model; streamed batches then
        // arrive as appends (rowsInserted at the end).
        connect(source, &QAbstractItemModel::modelReset, this,
                &LogFilterProxy::rebuild);
        connect(source, &QAbstractItemModel::rowsInserted, this,
                &LogFilterProxy::onSourceRowsInserted);
    }
    rebuild();
}

void LogFilterProxy::setQuery(const QString& text) {
    if (m_query == text)
        return;
    m_query = text;
    if (m_useRegex)
        m_regex = QRegularExpression(m_query,
                                     QRegularExpression::CaseInsensitiveOption);
    rebuild();
}

void LogFilterProxy::setUseRegex(bool enabled) {
    if (m_useRegex == enabled)
        return;
    m_useRegex = enabled;
    if (m_useRegex)
        m_regex = QRegularExpression(m_query,
                                     QRegularExpression::CaseInsensitiveOption);
    rebuild();
}

void LogFilterProxy::setLevelEnabled(LogModel::Level level, bool enabled) {
    const int idx = static_cast<int>(level);
    if (idx < 0 || idx >= LogModel::LevelCount || m_levelEnabled[idx] == enabled)
        return;
    m_levelEnabled[idx] = enabled;
    rebuild();
}

void LogFilterProxy::rebuild() {
    beginResetModel();

    m_rows.clear();
    m_sourceToProxy.clear();

    if (const QAbstractItemModel* src = sourceModel()) {
        const int n = src->rowCount();
        m_sourceToProxy.fill(-1, n);
        m_rows.reserve(n);
        for (int row = 0; row < n; ++row) {
            if (accepts(row)) {
                m_sourceToProxy[row] = static_cast<int>(m_rows.size());
                m_rows.append(row);
            }
        }
    }

    endResetModel();
}

void LogFilterProxy::onSourceRowsInserted(const QModelIndex& parent, int first,
                                          int last) {
    // The streaming loader only ever appends. If some other insert happens in
    // the middle, fall back to a full rebuild to stay correct.
    if (parent.isValid() || first != m_sourceToProxy.size()) {
        rebuild();
        return;
    }

    // Grow the reverse map for the new source rows (default: not visible).
    m_sourceToProxy.resize(last + 1, -1);

    QVector<int> accepted;
    for (int row = first; row <= last; ++row) {
        if (accepts(row))
            accepted.append(row);
    }
    if (accepted.isEmpty())
        return;

    const int proxyFirst = static_cast<int>(m_rows.size());
    beginInsertRows({}, proxyFirst, proxyFirst + accepted.size() - 1);
    for (int row : accepted) {
        m_sourceToProxy[row] = static_cast<int>(m_rows.size());
        m_rows.append(row);
    }
    endInsertRows();
}

bool LogFilterProxy::accepts(int sourceRow) const {
    const QAbstractItemModel* src = sourceModel();

    // Level filter (cheap: one role lookup, no text scan).
    const int level =
        src->index(sourceRow, LogModel::Col_Level)
            .data(LogModel::LevelRole)
            .toInt();
    if (level >= 0 && level < LogModel::LevelCount && !m_levelEnabled[level])
        return false;

    // Text filter on the message column.
    if (m_query.isEmpty())
        return true;

    const QString message =
        src->index(sourceRow, LogModel::Col_Message).data(Qt::DisplayRole).toString();
    if (m_useRegex)
        return m_regex.isValid() && m_regex.match(message).hasMatch();
    return message.contains(m_query, Qt::CaseInsensitive);
}

int LogFilterProxy::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int LogFilterProxy::columnCount(const QModelIndex& parent) const {
    if (parent.isValid() || !sourceModel())
        return 0;
    return sourceModel()->columnCount();
}

QModelIndex LogFilterProxy::index(int row, int column,
                                  const QModelIndex& parent) const {
    if (parent.isValid() || row < 0 || row >= m_rows.size())
        return {};
    return createIndex(row, column);
}

QModelIndex LogFilterProxy::parent(const QModelIndex& /*child*/) const {
    return {}; // flat table
}

QVariant LogFilterProxy::headerData(int section, Qt::Orientation orientation,
                                    int role) const {
    if (sourceModel() && orientation == Qt::Horizontal)
        return sourceModel()->headerData(section, orientation, role);
    return QAbstractProxyModel::headerData(section, orientation, role);
}

QModelIndex LogFilterProxy::mapToSource(const QModelIndex& proxyIndex) const {
    if (!proxyIndex.isValid() || !sourceModel() ||
        proxyIndex.row() >= m_rows.size())
        return {};
    return sourceModel()->index(m_rows[proxyIndex.row()], proxyIndex.column());
}

QModelIndex LogFilterProxy::mapFromSource(const QModelIndex& sourceIndex) const {
    if (!sourceIndex.isValid() || sourceIndex.row() >= m_sourceToProxy.size())
        return {};
    const int proxyRow = m_sourceToProxy[sourceIndex.row()];
    if (proxyRow < 0)
        return {}; // source row is filtered out
    return index(proxyRow, sourceIndex.column());
}
