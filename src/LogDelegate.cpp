#include "LogDelegate.h"

#include <QColor>

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
} // namespace

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
