#include "config_manager.h"

#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTextStream>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
{
    populateDefaults();
}

CurvePoints ConfigManager::defaultQuietPoints()
{
    return {
        {40.0, 0.0},
        {60.0, 0.0},
        {70.0, 45.0},
        {80.0, 65.0},
        {90.0, 90.0},
        {95.0, 100.0}
    };
}

CurvePoints ConfigManager::defaultBalancedPoints()
{
    return {
        {40.0, 0.0},
        {50.0, 0.0},
        {60.0, 45.0},
        {70.0, 65.0},
        {80.0, 85.0},
        {90.0, 100.0}
    };
}

CurvePoints ConfigManager::defaultAggressivePoints()
{
    return {
        {40.0, 0.0},
        {45.0, 40.0},
        {55.0, 60.0},
        {65.0, 80.0},
        {75.0, 100.0},
        {95.0, 100.0}
    };
}

void ConfigManager::populateDefaults()
{
    m_activeProfile = "Balanced";
    m_interval = 0.5;
    m_profiles.clear();

    m_profiles.insert("Quiet", {"Quiet", defaultQuietPoints()});
    m_profiles.insert("Balanced", {"Balanced", defaultBalancedPoints()});
    m_profiles.insert("Aggressive", {"Aggressive", defaultAggressivePoints()});
}

QString ConfigManager::activeProfile() const
{
    return m_activeProfile;
}

void ConfigManager::setActiveProfile(const QString &name)
{
    if (m_profiles.contains(name) && m_activeProfile != name) {
        m_activeProfile = name;
        emit activeProfileChanged(name);
    }
}

double ConfigManager::interval() const
{
    return m_interval;
}

void ConfigManager::setInterval(double sec)
{
    if (sec >= 0.05 && sec <= 10.0) {
        m_interval = sec;
    }
}

QStringList ConfigManager::profileNames() const
{
    return m_profiles.keys();
}

FanProfile ConfigManager::profile(const QString &name) const
{
    if (m_profiles.contains(name)) {
        return m_profiles.value(name);
    }
    return m_profiles.value("Balanced");
}

void ConfigManager::addOrUpdateProfile(const QString &name, const CurvePoints &points)
{
    CurvePoints sorted = points;
    std::sort(sorted.begin(), sorted.end(), [](const CurvePoint &a, const CurvePoint &b) {
        return a.first < b.first;
    });
    m_profiles.insert(name, {name, sorted});
}

bool ConfigManager::removeProfile(const QString &name)
{
    if (isDefaultProfile(name)) {
        return false;
    }
    if (m_profiles.remove(name) > 0) {
        if (m_activeProfile == name) {
            setActiveProfile("Balanced");
        }
        return true;
    }
    return false;
}

bool ConfigManager::isDefaultProfile(const QString &name) const
{
    return (name == "Quiet" || name == "Balanced" || name == "Aggressive");
}

bool ConfigManager::load(const QString &path)
{
    m_configPath = path;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        populateDefaults();
        emit configLoaded();
        return false;
    }

    populateDefaults();
    QTextStream in(&file);
    QString currentSection;
    QString currentProfile;

    static const QRegularExpression sectionRe(R"(^\[([^\]]+)\])");
    static const QRegularExpression keyValRe(R"(^([a-zA-Z0-9_-]+)\s*=\s*(.+)$)");
    static const QRegularExpression pointPairRe(R"(\[\s*([0-9.]+)\s*,\s*([0-9.]+)\s*\])");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        auto secMatch = sectionRe.match(line);
        if (secMatch.hasMatch()) {
            currentSection = secMatch.captured(1).trimmed();
            if (currentSection.startsWith("profiles.")) {
                currentProfile = currentSection.mid(QString("profiles.").length()).trimmed();
                if (!currentProfile.isEmpty() && !m_profiles.contains(currentProfile)) {
                    m_profiles.insert(currentProfile, {currentProfile, {}});
                }
            } else {
                currentProfile.clear();
            }
            continue;
        }

        auto kvMatch = keyValRe.match(line);
        if (kvMatch.hasMatch()) {
            QString key = kvMatch.captured(1).trimmed();
            QString val = kvMatch.captured(2).trimmed();

            if (currentSection == "fanboost") {
                if (key == "profile") {
                    val.remove('"');
                    if (!val.isEmpty()) {
                        m_activeProfile = val;
                    }
                } else if (key == "level") { // legacy
                    val.remove('"');
                    if (val == "low") m_activeProfile = "Quiet";
                    else if (val == "medium") m_activeProfile = "Balanced";
                    else if (val == "high") m_activeProfile = "Aggressive";
                } else if (key == "interval") {
                    bool ok = false;
                    double sec = val.toDouble(&ok);
                    if (ok && sec >= 0.05 && sec <= 10.0) {
                        m_interval = sec;
                    }
                }
            } else if (!currentProfile.isEmpty() && key == "points") {
                CurvePoints points;
                auto pointIt = pointPairRe.globalMatch(val);
                while (pointIt.hasNext()) {
                    auto pMatch = pointIt.next();
                    double t = pMatch.captured(1).toDouble();
                    double p = pMatch.captured(2).toDouble();
                    points.append({t, std::clamp(p, 0.0, 100.0)});
                }
                if (!points.isEmpty()) {
                    std::sort(points.begin(), points.end(), [](const CurvePoint &a, const CurvePoint &b) {
                        return a.first < b.first;
                    });
                    m_profiles.insert(currentProfile, {currentProfile, points});
                }
            }
        }
    }

    if (!m_profiles.contains(m_activeProfile)) {
        m_activeProfile = "Balanced";
    }

    emit configLoaded();
    return true;
}

QString ConfigManager::serializeToToml(const QString &activeProfile, double interval,
                                      const QMap<QString, FanProfile> &profiles)
{
    QString out;
    QTextStream ts(&out);

    ts << "# /etc/lenovo-fanboost.toml\n";
    ts << "# Managed by lenovo-fanboost-gui\n\n";
    ts << "[fanboost]\n";
    ts << "profile = \"" << activeProfile << "\"\n";
    ts << "interval = " << QString::number(interval, 'f', 2) << "\n\n";

    // Write default profiles first
    QStringList order = {"Quiet", "Balanced", "Aggressive"};
    for (const auto &k : profiles.keys()) {
        if (!order.contains(k)) {
            order.append(k);
        }
    }

    for (const QString &pName : order) {
        if (!profiles.contains(pName)) continue;
        const auto &prof = profiles.value(pName);
        ts << "[profiles." << prof.name << "]\n";
        ts << "points = [";
        for (int i = 0; i < prof.points.size(); ++i) {
            const auto &pt = prof.points[i];
            ts << "[" << QString::number(pt.first, 'f', 0) << ", "
               << QString::number(pt.second, 'f', 0) << "]";
            if (i < prof.points.size() - 1) {
                ts << ", ";
            }
        }
        ts << "]\n\n";
    }

    return out;
}

bool ConfigManager::save(const QString &path, QString *errorMessage)
{
    QString content = serializeToToml(m_activeProfile, m_interval, m_profiles);

    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        if (errorMessage) *errorMessage = "Could not create temporary file.";
        return false;
    }
    tempFile.write(content.toUtf8());
    tempFile.flush();
    QString tempPath = tempFile.fileName();
    tempFile.close();

    // Execute copy, chmod, and service restart in a single atomic pkexec prompt
    QProcess activeCheck;
    activeCheck.start("systemctl", QStringList() << "is-active" << "lenovo-fanboost.service");
    activeCheck.waitForFinished(500);
    bool isRunning = (activeCheck.readAllStandardOutput().trimmed() == "active");

    QString script;
    if (isRunning) {
        script = QString("cp %1 %2 && chmod 644 %2 && systemctl restart lenovo-fanboost.service")
                     .arg(tempPath, path);
    } else {
        script = QString("cp %1 %2 && chmod 644 %2").arg(tempPath, path);
    }

    QProcess proc;
    proc.start("pkexec", QStringList() << "sh" << "-c" << script);
    proc.waitForFinished(10000);

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        if (errorMessage) {
            *errorMessage = proc.readAllStandardError().trimmed();
            if (errorMessage->isEmpty()) {
                *errorMessage = proc.readAllStandardOutput().trimmed();
            }
            if (errorMessage->isEmpty()) {
                *errorMessage = "Privilege escalation was denied or cancelled.";
            }
        }
        return false;
    }

    return true;
}
