#include "MainWindow.h"
#include "LogModel.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    // Required so batches can cross from the loader thread to the model via a
    // queued signal/slot connection.
    qRegisterMetaType<QVector<LogModel::Entry>>();

    // Used by QSettings to persist window/session state (last opened dir, etc.).
    QCoreApplication::setOrganizationName(QStringLiteral("LogLens"));
    QCoreApplication::setApplicationName(QStringLiteral("LogLens"));

    MainWindow window;

    // Allow "LogLens some.log" to open a file directly.
    const QStringList args = app.arguments();
    if (args.size() > 1)
        window.openPath(args.at(1));

    window.show();
    return app.exec();
}
