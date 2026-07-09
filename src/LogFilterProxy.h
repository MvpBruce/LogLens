#pragma once

#include <QAbstractProxyModel>
#include <QRegularExpression>
#include <QVector>

#include "LogModel.h"

// A filtering proxy tuned for very large logs.
//
// QSortFilterProxyModel filters *incrementally*: hiding tens of thousands of
// scattered rows makes it emit tens of thousands of rowsRemoved ranges, and the
// view chokes processing them (the app appears to freeze). Instead, this proxy
// recomputes a compact "visible source rows" vector and publishes it with a
// single model reset — one O(N) scan, one signal, no per-row storm.
class LogFilterProxy : public QAbstractProxyModel {
    Q_OBJECT
public:
    explicit LogFilterProxy(QObject* parent = nullptr);

    void setSourceModel(QAbstractItemModel* source) override;

    void setQuery(const QString& text);
    void setUseRegex(bool enabled);
    void setLevelEnabled(LogModel::Level level, bool enabled);
    bool hasRegexError() const;
    QString regexErrorString() const;

    // QAbstractItemModel (flat table).
    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = {}) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role) const override;

    // QAbstractProxyModel.
    QModelIndex mapToSource(const QModelIndex& proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex& sourceIndex) const override;

private slots:
    void rebuild(); // recompute the visible-rows vector + reset
    // Streaming loads append rows at the end of the source; map just those.
    void onSourceRowsInserted(const QModelIndex& parent, int first, int last);

private:
    bool accepts(int sourceRow) const;

    QVector<int> m_rows;          // proxy row -> source row
    QVector<int> m_sourceToProxy; // source row -> proxy row (or -1)

    QString m_query;
    bool m_useRegex = false;
    QRegularExpression m_regex;
    bool m_levelEnabled[LogModel::LevelCount];
};
