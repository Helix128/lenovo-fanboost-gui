#include "config_manager.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryFile>
#include <QTextStream>
#include <cmath>
#include <limits>

namespace
{
QString tomlString(const QString &value)
{
    QString escaped = value;
    escaped.replace('\\', "\\\\");
    escaped.replace('"', "\\\"");
    escaped.replace('\n', "\\n");
    escaped.replace('\r', "\\r");
    escaped.replace('\t', "\\t");
    return '"' + escaped + '"';
}

bool parseTomlString(QString value, QString *result)
{
    value = value.trimmed();
    if (value.size() < 2 || value.front() != '"' || value.back() != '"')
        return false;
    QString decoded;
    for (qsizetype i = 1; i < value.size() - 1; ++i) {
        QChar ch = value.at(i);
        if (ch != '\\') {
            decoded.append(ch);
            continue;
        }
        if (++i >= value.size() - 1)
            return false;
        ch = value.at(i);
        if (ch == '"' || ch == '\\')
            decoded.append(ch);
        else if (ch == 'n')
            decoded.append('\n');
        else if (ch == 'r')
            decoded.append('\r');
        else if (ch == 't')
            decoded.append('\t');
        else
            return false;
    }
    *result = decoded;
    return true;
}

QString stripInlineComment(const QString &line)
{
    bool quoted = false;
    bool escaped = false;
    for (qsizetype i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                quoted = false;
        } else if (ch == '"') {
            quoted = true;
        } else if (ch == '#') {
            return line.left(i);
        }
    }
    return line;
}

bool parseSectionHeader(const QString &line, QString *section)
{
    if (!line.startsWith('['))
        return false;
    bool quoted = false;
    bool escaped = false;
    for (qsizetype i = 1; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (quoted) {
            if (escaped)
                escaped = false;
            else if (ch == '\\')
                escaped = true;
            else if (ch == '"')
                quoted = false;
        } else if (ch == '"') {
            quoted = true;
        } else if (ch == ']') {
            const QString trailing = line.mid(i + 1).trimmed();
            if (!trailing.isEmpty() && !trailing.startsWith('#'))
                return false;
            *section = line.mid(1, i - 1).trimmed();
            return !quoted;
        }
    }
    return false;
}

bool validProfileName(const QString &name)
{
    if (name.trimmed().isEmpty() || name.size() > 64)
        return false;
    for (QChar ch : name) {
        if (ch.isNull() || ch.category() == QChar::Other_Control)
            return false;
    }
    return true;
}

bool validPoints(const CurvePoints &points)
{
    if (points.size() < 2 || points.size() > 64)
        return false;
    double previousTemp = -std::numeric_limits<double>::infinity();
    double previousPercent = -std::numeric_limits<double>::infinity();
    for (const CurvePoint &point : points) {
        if (!std::isfinite(point.first) || !std::isfinite(point.second) || point.first < 30.0 ||
            point.first > 100.0 || point.second < 0.0 || point.second > 100.0 ||
            point.first <= previousTemp || point.second < previousPercent)
            return false;
        previousTemp = point.first;
        previousPercent = point.second;
    }
    return true;
}

const char privilegedSaveScript[] =
    "set -eu\n"
    "new=; backup=; verify=; preserve_backup=0\n"
    "cleanup() { [ -z \"$new\" ] || rm -f -- \"$new\"; if [ \"$preserve_backup\" = 0 ]; then [ -z "
    "\"$backup\" ] || rm -f -- \"$backup\"; [ -z \"$verify\" ] || rm -f -- \"$verify\"; fi; }\n"
    "trap cleanup EXIT\n"
    "trap 'exit 130' HUP INT TERM\n"
    "src=$1\n"
    "dst=$2\n"
    "restart=$3\n"
    "expected=$4\n"
    "unchanged() { if [ \"$expected\" = missing ]; then [ ! -e \"$dst\" ] && [ ! -L \"$dst\" ]; else [ -f "
    "\"$dst\" ] && [ ! -L \"$dst\" ] || return 1; actual=$(sha256sum -- \"$dst\"); [ \"${actual%% *}\" = "
    "\"$expected\" ]; fi; }\n"
    "if ! unchanged; then echo CONFIG_CONFLICT; exit 22; fi\n"
    "dir=${dst%/*}; [ \"$dir\" != \"$dst\" ] || dir=.\n"
    "new=$(mktemp \"$dir/.lenovo-fanboost.XXXXXX\")\n"
    "backup=$(mktemp \"$dir/.lenovo-fanboost-backup.XXXXXX\")\n"
    "verify=$(mktemp \"$dir/.lenovo-fanboost-verify.XXXXXX\")\n"
    "had_old=0\n"
    "if [ -e \"$dst\" ]; then cp -p -- \"$dst\" \"$backup\"; cp -p -- \"$dst\" \"$verify\"; had_old=1; fi\n"
    "if ! cp -- \"$src\" \"$new\" || ! chmod 644 \"$new\"; then exit 10; fi\n"
    "if ! unchanged; then echo CONFIG_CONFLICT; exit 22; fi\n"
    "if ! mv -f -- \"$new\" \"$dst\"; then exit 10; fi\n"
    "echo CONFIG_SAVED\n"
    "if [ \"$restart\" = yes ]; then\n"
    "  if systemctl restart lenovo-fanboost.service; then echo SERVICE_RESTARTED; exit 0; fi\n"
    "  if [ \"$had_old\" = 1 ]; then\n"
    "    if ! mv -f -- \"$backup\" \"$dst\" || ! cmp -s -- \"$dst\" \"$verify\"; then preserve_backup=1; "
    "echo CONFIG_RESTORE_UNVERIFIED:$backup:$verify; exit 21; fi\n"
    "  else\n"
    "    if ! rm -f -- \"$dst\" || [ -e \"$dst\" ]; then echo CONFIG_RESTORE_UNVERIFIED; exit 21; fi\n"
    "  fi\n"
    "  systemctl restart lenovo-fanboost.service >/dev/null 2>&1 || true\n"
    "  echo CONFIG_RESTORED_VERIFIED; exit 20\n"
    "fi\n"
    "exit 0\n";
} // namespace

ConfigManager::ConfigManager(QObject *parent) : QObject(parent)
{
    populateDefaults();
    m_processTimeout.setSingleShot(true);
    connect(&m_processTimeout, &QTimer::timeout, this, [this] {
        m_processTimedOut = true;
        m_saveProcess.terminate();
        QTimer::singleShot(2000, this, [this] {
            if (m_processTimedOut && m_saveProcess.state() != QProcess::NotRunning)
                m_saveProcess.kill();
        });
    });
    connect(&m_saveProcess, &QProcess::started, this, [this] {
        m_processTimedOut = false;
        m_processTimeout.start(m_saveProcess.property("operation").toString() == "service-check" ? 2000
                                                                                                 : 60000);
    });
    connect(&m_saveProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus status) {
                m_processTimeout.stop();
                handleSaveProcessFinished(code, status);
            });
    connect(&m_saveProcess, &QProcess::started, this, [this] {
        if (m_saveProcess.property("operation").toString() == "save") {
            m_saveProcess.write(privilegedSaveScript, sizeof(privilegedSaveScript) - 1);
            m_saveProcess.closeWriteChannel();
        }
    });
    connect(&m_saveProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || !m_busy)
            return;
        if (m_saveProcess.property("operation").toString() == "service-check") {
            finishSave(
                false,
                "Could not determine whether the service is active. Reload the configuration and try again.");
        } else {
            finishSave(false, "Privilege authorization was denied, cancelled, or could not be started.");
        }
    });
}

CurvePoints ConfigManager::defaultQuietPoints()
{
    return {{40.0, 0.0}, {60.0, 0.0}, {70.0, 45.0}, {80.0, 65.0}, {90.0, 90.0}, {95.0, 100.0}};
}

CurvePoints ConfigManager::defaultBalancedPoints()
{
    return {{40.0, 0.0}, {50.0, 0.0}, {60.0, 45.0}, {70.0, 65.0}, {80.0, 85.0}, {90.0, 100.0}};
}

CurvePoints ConfigManager::defaultAggressivePoints()
{
    return {{40.0, 0.0}, {45.0, 40.0}, {55.0, 60.0}, {65.0, 80.0}, {75.0, 100.0}, {95.0, 100.0}};
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

QString ConfigManager::activeProfile() const { return m_activeProfile; }

void ConfigManager::setActiveProfile(const QString &name)
{
    if (m_profiles.contains(name) && m_activeProfile != name) {
        m_activeProfile = name;
        emit activeProfileChanged(name);
    }
}

double ConfigManager::interval() const { return m_interval; }

void ConfigManager::setInterval(double sec)
{
    if (sec >= 0.05 && sec <= 10.0) {
        m_interval = sec;
    }
}

QStringList ConfigManager::profileNames() const { return m_profiles.keys(); }

FanProfile ConfigManager::profile(const QString &name) const
{
    if (m_profiles.contains(name)) {
        return m_profiles.value(name);
    }
    return m_profiles.value("Balanced");
}

void ConfigManager::addOrUpdateProfile(const QString &name, const CurvePoints &points)
{
    if (validProfileName(name) && validPoints(points))
        m_profiles.insert(name, {name, points});
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
        m_fingerprintInitialized = true;
        m_loadedConfigExisted = QFileInfo::exists(path);
        m_loadedConfigBytes.clear();
        m_loadValid = !m_loadedConfigExisted;
        populateDefaults();
        emit configLoaded();
        return false;
    }

    m_loadedConfigBytes = file.readAll();
    m_loadedConfigExisted = true;
    m_fingerprintInitialized = true;
    const QString configText = QString::fromUtf8(m_loadedConfigBytes);
    file.close();

    populateDefaults();
    QTextStream in(configText.toUtf8());
    QString currentSection;
    QString currentProfile;
    QSet<QString> seenProfileNames;
    QSet<QString> seenAssignments;
    bool configValid = configText.toUtf8() == m_loadedConfigBytes;
    bool seenFanboostSection = false;

    static const QRegularExpression keyValRe(R"rx(^([a-zA-Z0-9_-]+)\s*=\s*(.+)$)rx");
    static const QRegularExpression pointArrayRe(
        R"rx(^\s*\[\s*(?:\[\s*-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\s*,\s*-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\s*\]\s*(?:,\s*\[\s*-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\s*,\s*-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\s*\]\s*)*)?\]\s*$)rx");
    static const QRegularExpression pointPairRe(
        R"rx(\[\s*(-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)\s*,\s*(-?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?)\s*\])rx");

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }

        QString parsedSection;
        if (parseSectionHeader(line, &parsedSection)) {
            currentSection = parsedSection;
            if (currentSection.startsWith("profiles.")) {
                const QString encodedName = currentSection.mid(QString("profiles.").length()).trimmed();
                if (encodedName.startsWith('"')) {
                    QString parsedName;
                    if (parseTomlString(encodedName, &parsedName))
                        currentProfile = parsedName;
                    else {
                        currentProfile.clear();
                        configValid = false;
                    }
                } else {
                    currentProfile = encodedName;
                }
                if (validProfileName(currentProfile) && seenProfileNames.contains(currentProfile)) {
                    configValid = false;
                    currentProfile.clear();
                } else if (validProfileName(currentProfile)) {
                    seenProfileNames.insert(currentProfile);
                    if (!m_profiles.contains(currentProfile))
                        m_profiles.insert(currentProfile, {currentProfile, {}});
                } else if (!validProfileName(currentProfile)) {
                    configValid = false;
                    currentProfile.clear();
                }
            } else {
                currentProfile.clear();
                if (currentSection == "fanboost") {
                    if (seenFanboostSection)
                        configValid = false;
                    seenFanboostSection = true;
                } else {
                    configValid = false;
                }
            }
            continue;
        }

        line = stripInlineComment(line).trimmed();
        if (line.isEmpty())
            continue;

        auto kvMatch = keyValRe.match(line);
        if (kvMatch.hasMatch()) {
            QString key = kvMatch.captured(1).trimmed();
            QString val = kvMatch.captured(2).trimmed();
            const QString assignment = currentSection + QChar('.') + key;
            if (seenAssignments.contains(assignment))
                configValid = false;
            seenAssignments.insert(assignment);

            if (currentSection == "fanboost") {
                if (key == "profile") {
                    QString parsed;
                    if (parseTomlString(val, &parsed) && validProfileName(parsed))
                        m_activeProfile = parsed;
                    else
                        configValid = false;
                } else if (key == "level") { // legacy
                    QString parsed;
                    if (parseTomlString(val, &parsed))
                        val = parsed;
                    else
                        val.remove('"');
                    if (val == "low")
                        m_activeProfile = "Quiet";
                    else if (val == "medium")
                        m_activeProfile = "Balanced";
                    else if (val == "high")
                        m_activeProfile = "Aggressive";
                    else
                        configValid = false;
                } else if (key == "interval") {
                    bool ok = false;
                    double sec = val.toDouble(&ok);
                    if (ok && sec >= 0.05 && sec <= 10.0) {
                        m_interval = sec;
                    } else
                        configValid = false;
                } else {
                    configValid = false;
                }
            } else if (!currentProfile.isEmpty() && key == "points") {
                CurvePoints points;
                auto pointIt = pointPairRe.globalMatch(val);
                while (pointIt.hasNext()) {
                    auto pMatch = pointIt.next();
                    double t = pMatch.captured(1).toDouble();
                    double p = pMatch.captured(2).toDouble();
                    points.append({t, p});
                }
                if (pointArrayRe.match(val).hasMatch() && validPoints(points)) {
                    m_profiles.insert(currentProfile, {currentProfile, points});
                } else {
                    configValid = false;
                }
            } else {
                configValid = false;
            }
        } else {
            configValid = false;
        }
    }

    if (!m_profiles.contains(m_activeProfile))
        configValid = false;
    for (auto it = m_profiles.cbegin(); it != m_profiles.cend(); ++it) {
        if (!validPoints(it.value().points))
            configValid = false;
    }
    if (!configValid) {
        populateDefaults();
    }
    m_loadValid = configValid;

    emit configLoaded();
    return configValid;
}

QString ConfigManager::serializeToToml(const QString &activeProfile, double interval,
                                       const QMap<QString, FanProfile> &profiles)
{
    if (!validProfileName(activeProfile) || !std::isfinite(interval) || interval < 0.05 || interval > 10.0)
        return {};
    if (!profiles.contains(activeProfile))
        return {};
    for (auto it = profiles.cbegin(); it != profiles.cend(); ++it) {
        if (!validProfileName(it.key()) || it.value().name != it.key() || !validPoints(it.value().points))
            return {};
    }
    QString out;
    QTextStream ts(&out);

    ts << "# /etc/lenovo-fanboost.toml\n";
    ts << "# Managed by lenovo-fanboost-gui\n\n";
    ts << "[fanboost]\n";
    ts << "profile = " << tomlString(activeProfile) << "\n";
    ts << "interval = " << QString::number(interval, 'g', 15) << "\n\n";

    // Write default profiles first
    QStringList order = {"Quiet", "Balanced", "Aggressive"};
    for (const auto &k : profiles.keys()) {
        if (!order.contains(k)) {
            order.append(k);
        }
    }

    for (const QString &pName : order) {
        if (!profiles.contains(pName))
            continue;
        const auto &prof = profiles.value(pName);
        ts << "[profiles." << tomlString(prof.name) << "]\n";
        ts << "points = [";
        for (int i = 0; i < prof.points.size(); ++i) {
            const auto &pt = prof.points[i];
            ts << "[" << QString::number(pt.first, 'g', 15) << ", " << QString::number(pt.second, 'g', 15)
               << "]";
            if (i < prof.points.size() - 1) {
                ts << ", ";
            }
        }
        ts << "]\n\n";
    }

    return out;
}

void ConfigManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(busy);
}

void ConfigManager::finishSave(bool success, const QString &message, bool serviceRestarted)
{
    m_saveTempFile.remove();
    setBusy(false);
    emit saveFinished(success, message, serviceRestarted);
}

void ConfigManager::saveAsync(const QString &path)
{
    if (m_busy) {
        emit saveFinished(false, "A configuration operation is already in progress.", false);
        return;
    }
    if (!m_loadValid) {
        emit saveFinished(false,
                          "The loaded configuration is invalid. Reload or correct it before saving; it was "
                          "not overwritten.",
                          false);
        return;
    }
    if (!m_fingerprintInitialized || path != m_configPath) {
        emit saveFinished(
            false, "Reload this configuration path before saving so external changes can be checked.", false);
        return;
    }
    const QFileInfo currentInfo(path);
    if (currentInfo.exists() != m_loadedConfigExisted) {
        emit saveFinished(false, "The configuration changed since it was loaded. Reload it before saving.",
                          false);
        return;
    }
    if (m_loadedConfigExisted) {
        QFile currentFile(path);
        if (!currentFile.open(QIODevice::ReadOnly) || currentFile.readAll() != m_loadedConfigBytes) {
            emit saveFinished(
                false, "The configuration changed since it was loaded. Reload it before saving.", false);
            return;
        }
    }
    m_saveContent = serializeToToml(m_activeProfile, m_interval, m_profiles);
    if (m_saveContent.isEmpty()) {
        emit saveFinished(false, "Configuration contains an invalid profile name, interval, or curve.",
                          false);
        return;
    }
    m_savePath = path;
    setBusy(true);
    m_saveTempFile.setAutoRemove(true);
    if (!m_saveTempFile.open() ||
        m_saveTempFile.write(m_saveContent.toUtf8()) != m_saveContent.toUtf8().size() ||
        !m_saveTempFile.flush()) {
        finishSave(false, "Could not prepare the configuration file.");
        return;
    }
    m_saveTempFile.close();
    m_saveProcess.setProperty("operation", "service-check");
    m_saveProcess.start(
        "systemctl", {"show", "--property=LoadState", "--property=ActiveState", "lenovo-fanboost.service"});
}

void ConfigManager::startPrivilegedSave()
{
    if (!m_busy || m_saveProcess.state() != QProcess::NotRunning)
        return;
    m_saveProcess.setProperty("operation", "save");
    const QString tempPath = m_saveTempFile.fileName();
    const QString expected =
        m_loadedConfigExisted
            ? QString::fromLatin1(
                  QCryptographicHash::hash(m_loadedConfigBytes, QCryptographicHash::Sha256).toHex())
            : QStringLiteral("missing");
    m_saveProcess.start(
        "pkexec", {"/bin/sh", "-s", "--", tempPath, m_savePath, m_restartAfterSave ? "yes" : "no", expected});
}

void ConfigManager::handleSaveProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (!m_busy)
        return;
    if (m_saveProcess.property("operation").toString() == "service-check") {
        const QString output = QString::fromLocal8Bit(m_saveProcess.readAllStandardOutput());
        QString loadState;
        QString activeState;
        for (const QString &line : output.split('\n')) {
            if (line.startsWith("LoadState="))
                loadState = line.mid(10).trimmed();
            else if (line.startsWith("ActiveState="))
                activeState = line.mid(12).trimmed();
        }
        if (m_processTimedOut || exitStatus != QProcess::NormalExit || exitCode != 0 ||
            loadState != "loaded" || activeState.isEmpty()) {
            finishSave(false,
                       "Could not determine a valid service state. Reload the configuration and try again.");
            return;
        }
        m_restartAfterSave = activeState == "active";
        startPrivilegedSave();
        return;
    }

    const QString output = QString::fromLocal8Bit(m_saveProcess.readAllStandardOutput()).trimmed();
    const QString error = QString::fromLocal8Bit(m_saveProcess.readAllStandardError()).trimmed();
    if (m_processTimedOut) {
        finishSave(false, "Configuration operation timed out.");
        return;
    }
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        m_loadedConfigBytes = m_saveContent.toUtf8();
        m_loadedConfigExisted = true;
        m_fingerprintInitialized = true;
        m_loadValid = true;
        finishSave(true, "Configuration saved.", m_restartAfterSave);
    } else if (exitStatus == QProcess::NormalExit && exitCode == 22) {
        finishSave(false, "The configuration changed while authorization was pending. Reload it before "
                          "saving; it was not overwritten.");
    } else if (exitStatus == QProcess::NormalExit && exitCode == 20) {
        finishSave(false,
                   "The service restart failed. The previous configuration was restored and verified. " +
                       error,
                   false);
    } else if (exitStatus == QProcess::NormalExit && exitCode == 21) {
        finishSave(false,
                   "The service restart failed and restoration could not be verified. Inspect the recovery "
                   "files reported by the helper, then reload before saving. " +
                       output,
                   false);
    } else {
        finishSave(false,
                   error.isEmpty()
                       ? "Configuration could not be saved; authorization may have been cancelled. " + output
                       : error);
    }
}
