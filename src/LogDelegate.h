#pragma once

#include <QStyledItemDelegate>

// Colors each row by severity (read from LogModel::LevelRole). Keeping this in a
// delegate lets the model stay a pure data container — presentation lives here.
class LogDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

protected:
    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override;
};
