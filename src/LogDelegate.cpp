#include "LogDelegate.h"

#include <algorithm>

#include <QApplication>
#include <QColor>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>

#include "LogModel.h"

namespace {
// Returns an invalid QColor for levels that use the default text color.
QColor colorForLevel(int level) {
    switch (static_cast<LogModel::Level>(level)) {
    case LogModel::Level::Error:
        return QColor(0xE0, 0x5A, 0x5A);
    case LogModel::Level::Warn:
        return QColor(0xD6, 0xA5, 0x3C);
    case LogModel::Level::Debug:
    case LogModel::Level::Trace:
        return QColor(0x8A, 0x8A, 0x8A);
    default:
        return {};
    }
}

void addRegexRanges(QVector<QPair<int, int>>* ranges, const QString& text,
                    const QRegularExpression& regex) {
    if (!regex.isValid())
        return;

    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const int start = match.capturedStart();
        const int length = match.capturedLength();
        if (start >= 0 && length > 0)
            ranges->append({start, length});
    }
}

bool rangeLess(const QPair<int, int>& a, const QPair<int, int>& b) {
    if (a.first == b.first)
        return a.second > b.second;
    return a.first < b.first;
}
} // namespace

void LogDelegate::setFilterHighlight(const QString& text, bool useRegex) {
    m_filterText = text;
    m_filterUsesRegex = useRegex;
    m_filterRegex = useRegex
                        ? QRegularExpression(
                              text, QRegularExpression::CaseInsensitiveOption)
                        : QRegularExpression();
}

void LogDelegate::setFindHighlight(const QString& text) {
    m_findText = text;
}

void LogDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const {
    if (index.column() != LogModel::Col_Message) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    const QString text = index.data(Qt::DisplayRole).toString();
    const QVector<QPair<int, int>> ranges = matchRanges(text);
    if (ranges.isEmpty()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    opt.text.clear();

    QStyle* style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    const QRect textRect =
        style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);
    const QFontMetrics fm(opt.font);
    const int top = textRect.top() + 2;
    const int height = qMax(1, textRect.height() - 4);

    painter->save();
    painter->setClipRect(textRect);

    const QColor filterColor(0xFF, 0xE6, 0x8A, 170);
    const QColor findColor(0x8D, 0xD8, 0xFF, 190);

    for (const auto& range : ranges) {
        const int start = range.first;
        const int length = range.second;
        const int x = textRect.left() + fm.horizontalAdvance(text.left(start));
        const int width = fm.horizontalAdvance(text.mid(start, length));
        const bool isFind =
            !m_findText.isEmpty() &&
            text.mid(start, length).compare(m_findText, Qt::CaseInsensitive) ==
                0;
        painter->fillRect(QRect(x, top, width, height),
                          isFind ? findColor : filterColor);
    }

    const bool selected = opt.state & QStyle::State_Selected;
    painter->setPen(opt.palette.color(
        selected ? QPalette::HighlightedText : QPalette::Text));
    painter->drawText(textRect, opt.displayAlignment, text);
    painter->restore();
}

void LogDelegate::initStyleOption(QStyleOptionViewItem* option,
                                  const QModelIndex& index) const {
    QStyledItemDelegate::initStyleOption(option, index);

    const QColor color = colorForLevel(index.data(LogModel::LevelRole).toInt());
    if (color.isValid()) {
        // Set both so the color is right whether or not the row is selected.
        option->palette.setColor(QPalette::Text, color);
        option->palette.setColor(QPalette::HighlightedText, color);
    }
}

QVector<QPair<int, int>> LogDelegate::matchRanges(const QString& text) const {
    QVector<QPair<int, int>> ranges;
    if (text.isEmpty())
        return ranges;

    if (!m_filterText.isEmpty()) {
        if (m_filterUsesRegex)
            addRegexRanges(&ranges, text, m_filterRegex);
        else
            appendLiteralRanges(&ranges, text, m_filterText);
    }
    appendLiteralRanges(&ranges, text, m_findText);

    std::sort(ranges.begin(), ranges.end(), rangeLess);
    return ranges;
}

void LogDelegate::appendLiteralRanges(QVector<QPair<int, int>>* ranges,
                                      const QString& text,
                                      const QString& needle) const {
    if (needle.isEmpty())
        return;

    int pos = 0;
    while (true) {
        pos = text.indexOf(needle, pos, Qt::CaseInsensitive);
        if (pos < 0)
            break;
        ranges->append({pos, needle.size()});
        pos += qMax(1, needle.size());
    }
}
