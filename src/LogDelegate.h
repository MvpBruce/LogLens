#pragma once

#include <QRegularExpression>
#include <QStyledItemDelegate>

// Colors each row by severity and highlights filter/find matches in messages.
// Presentation stays here so LogModel can remain a pure data container.
class LogDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void setFilterHighlight(const QString& text, bool useRegex);
    void setFindHighlight(const QString& text);

protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override;

private:
    QVector<QPair<int, int>> matchRanges(const QString& text) const;
    void appendLiteralRanges(QVector<QPair<int, int>>* ranges,
                             const QString& text,
                             const QString& needle) const;

    QString m_filterText;
    bool m_filterUsesRegex = false;
    QRegularExpression m_filterRegex;
    QString m_findText;
};
