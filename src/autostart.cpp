#include "autostart.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

QString Autostart::filePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) +
           QStringLiteral("/autostart/lenovo-fanboost-gui.desktop");
}

bool Autostart::enabled()
{
    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const auto content = file.readAll();
    return content.contains("X-Lenovo-FanBoost-Managed=true") && !content.contains("\nHidden=true");
}

bool Autostart::setEnabled(bool enabled, QString *error)
{
    auto fail = [error](const QString &message) {
        if (error)
            *error = message;
        return false;
    };
    QFile existing(filePath());
    if (existing.exists()) {
        if (!existing.open(QIODevice::ReadOnly) ||
            !existing.readAll().contains("X-Lenovo-FanBoost-Managed=true"))
            return fail(QStringLiteral(
                "A custom startup entry already exists. Edit it in your desktop's startup settings."));
        existing.close();
    }
    if (!enabled) {
        return !existing.exists() || existing.remove() ||
               fail(QStringLiteral("Cannot remove the login startup entry. Check its permissions."));
    }
    const QString directory = QFileInfo(filePath()).absolutePath();
    if (!QDir().mkpath(directory))
        return fail(QStringLiteral("Cannot create the login startup directory."));
    QString executable = QCoreApplication::applicationFilePath();
    executable.replace('%', "%%");
    executable.replace('\\', "\\\\\\\\");
    executable.replace('"', "\\\\\\\"");
    executable.replace('$', "\\\\$");
    executable.replace('`', "\\\\`");
    const QByteArray content = QStringLiteral("[Desktop Entry]\nType=Application\nName=Lenovo Fan Boost\n"
                                              "Exec=\"%1\" --tray\nIcon=lenovo-fanboost-gui\n"
                                              "Terminal=false\nX-Lenovo-FanBoost-Managed=true\n")
                                   .arg(executable)
                                   .toUtf8();
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || file.write(content) != content.size() || !file.commit())
        return fail(QStringLiteral("Cannot save the login startup entry. Check directory permissions."));
    return true;
}
