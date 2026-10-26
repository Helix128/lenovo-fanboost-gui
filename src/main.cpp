#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <unistd.h>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("lenovo-fanboost-gui");
    app.setApplicationDisplayName("Lenovo Fan Boost");
    app.setOrganizationName("Helix128");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("lenovo-fanboost-gui");
    app.setWindowIcon(QIcon(":/icons/lenovo-fanboost-gui-128.png"));

    const QString socketName = QString("lenovo-fanboost-gui-single-%1").arg(getuid());

    // 1. Check if an instance is already running
    QLocalSocket socket;
    socket.connectToServer(socketName);
    if (socket.waitForConnected(500)) {
        // Signal existing instance to raise its window and exit immediately
        socket.write("ACTIVATE\n");
        socket.waitForBytesWritten(500);
        return 0;
    }

    // 2. Start local single-instance listener
    QLocalServer::removeServer(socketName);
    auto *server = new QLocalServer(&app);
    server->listen(socketName);

    QCommandLineParser parser;
    parser.setApplicationDescription("Lenovo Fan Boost GUI Companion");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption trayOption(QStringList() << "t" << "tray" << "minimized",
                                  "Start minimized to system tray.");
    parser.addOption(trayOption);
    parser.process(app);

    MainWindow window;

    // Reactivate window if user launches a second instance
    QObject::connect(server, &QLocalServer::newConnection, [&window, server]() {
        QLocalSocket *client = server->nextPendingConnection();
        if (client) {
            client->waitForReadyRead(500);
            window.showNormal();
            window.activateWindow();
            window.raise();
            client->disconnectFromServer();
            client->deleteLater();
        }
    });

    if (!parser.isSet(trayOption)) {
        window.show();
    }

    return app.exec();
}
