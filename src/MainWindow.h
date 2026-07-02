#pragma once

#include <QMainWindow>

class LogModel;
class QTableView;

// Main editor window: a table view of the loaded log plus File > Open.
// Filtering (D5), background loading (D6), and live tail (D7) hang off here.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Loads a file into the view (used by File > Open, drag-drop, and CLI arg).
    void openPath(const QString& path);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void onOpen();

private:
    LogModel* m_model;
    QTableView* m_view;
};
