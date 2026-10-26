#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QSystemTrayIcon>
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

    QIcon appIcon;
    appIcon.addFile(":/icons/lenovo-fanboost-gui-16.png", QSize(16, 16));
    appIcon.addFile(":/icons/lenovo-fanboost-gui-24.png", QSize(24, 24));
    appIcon.addFile(":/icons/lenovo-fanboost-gui-32.png", QSize(32, 32));
    appIcon.addFile(":/icons/lenovo-fanboost-gui-48.png", QSize(48, 48));
    appIcon.addFile(":/icons/lenovo-fanboost-gui-64.png", QSize(64, 64));
    appIcon.addFile(":/icons/lenovo-fanboost-gui-128.png", QSize(128, 128));
    appIcon.addFile(":/icons/lenovo-fanboost-gui-256.png", QSize(256, 256));
    appIcon.addFile(":/icons/lenovo-fanboost-gui.png", QSize(512, 512));
    app.setWindowIcon(appIcon);

    QCommandLineParser parser;
    parser.setApplicationDescription("Fan profiles and curve editor for lenovo-fanboost.");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption trayOption(QStringList() << "t" << "tray" << "minimized",
                                  "Start in the system tray, when available.");
    QCommandLineOption demoOption("demo", "Preview the interface with sample data and no hardware control.");
    parser.addOption(trayOption);
    parser.addOption(demoOption);
    parser.process(app);
    const bool demo = parser.isSet(demoOption);
    if (demo) {
        app.setApplicationName("lenovo-fanboost-gui-preview");
    }

    const QString socketName = QString("lenovo-fanboost-gui-single-%1").arg(getuid());

    // 1. Check if an instance is already running
    QLocalSocket socket;
    if (!demo)
        socket.connectToServer(socketName);
    if (!demo && socket.waitForConnected(500)) {
        // Signal existing instance to raise its window and exit immediately
        socket.write("ACTIVATE\n");
        socket.waitForBytesWritten(500);
        return 0;
    }

    // 2. Start local single-instance listener
    auto *server = new QLocalServer(&app);
    server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!demo) {
        QLocalServer::removeServer(socketName);
        if (!server->listen(socketName)) {
            QMessageBox::critical(nullptr, "Cannot start Lenovo Fan Boost", server->errorString());
            return 1;
        }
    }

    MainWindow window(nullptr, demo);

    // Reactivate window if user launches a second instance
    QObject::connect(server, &QLocalServer::newConnection, [&window, server]() {
        QLocalSocket *client = server->nextPendingConnection();
        if (client) {
            window.showNormal();
            window.activateWindow();
            window.raise();
            client->disconnectFromServer();
            client->deleteLater();
        }
    });

    if (demo || !parser.isSet(trayOption) || !QSystemTrayIcon::isSystemTrayAvailable()) {
        window.show();
    }

    return app.exec();
}
