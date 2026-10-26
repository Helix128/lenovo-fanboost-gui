#include "profile_io.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
bool valid(const FanProfile &profile)
{
    return !ConfigManager::serializeToToml(profile.name, 0.5, {{profile.name, profile}}).isEmpty();
}
bool fail(QString *error, const QString &message)
{
    if (error)
        *error = message;
    return false;
}
} // namespace

bool ProfileIO::read(const QString &path, FanProfile *profile, QString *error)
{
    QFile file(path);
    if (!profile || !file.open(QIODevice::ReadOnly))
        return fail(error, QStringLiteral("Cannot read the profile file. Check its path and permissions."));
    if (file.size() > 65536)
        return fail(error, QStringLiteral("Profile files must be smaller than 64 KiB."));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.read(65537), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return fail(error, QStringLiteral("The file is not a valid JSON profile."));
    const auto object = document.object();
    if (object.value("version").toDouble(-1) != 1 || !object.value("name").isString() ||
        !object.value("points").isArray())
        return fail(error, QStringLiteral("Expected a version 1 profile with a name and curve points."));
    FanProfile candidate{object.value("name").toString(), {}};
    for (const auto point : object.value("points").toArray()) {
        if (!point.isArray() || point.toArray().size() != 2 || !point.toArray()[0].isDouble() ||
            !point.toArray()[1].isDouble())
            return fail(error, QStringLiteral("Each curve point needs a temperature and fan percentage."));
        candidate.points.append({point.toArray()[0].toDouble(), point.toArray()[1].toDouble()});
    }
    if (!valid(candidate))
        return fail(error, QStringLiteral("Invalid name or curve. Use 2–64 increasing temperatures from 30 "
                                          "to 100 °C and nondecreasing fan levels from 0 to 100%."));
    *profile = candidate;
    return true;
}

bool ProfileIO::write(const QString &path, const FanProfile &profile, QString *error)
{
    if (!valid(profile))
        return fail(error, QStringLiteral("Correct the profile name or curve before exporting."));
    QJsonArray points;
    for (const auto &point : profile.points)
        points.append(QJsonArray{point.first, point.second});
    const QJsonDocument document(QJsonObject{{"version", 1}, {"name", profile.name}, {"points", points}});
    QSaveFile file(path);
    const QByteArray bytes = document.toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, QStringLiteral("Cannot export this profile. Choose a writable location."));
    return true;
}
