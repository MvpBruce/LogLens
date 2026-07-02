#pragma once

#include <QMainWindow>

class LogModel;
class LogFilterProxy;
class QLineEdit;
class QTableView;
class QTimer;

// Main editor window: a filtered table view of the loaded log plus File > Open.
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
    void buildFilterBar(); // toolbar: text query, regex, level checkboxes
    void updateStatus();   // "showing X of N lines"

    LogModel* m_model;
    LogFilterProxy* m_proxy;
    QTableView* m_view;
    QLineEdit* m_query = nullptr;
    QTimer* m_filterTimer = nullptr; // debounces text filtering
};
