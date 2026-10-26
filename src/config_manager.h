#pragma once

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QProcess>
#include <QString>
#include <QTemporaryFile>
#include <QTimer>

using CurvePoint = QPair<double, double>; // <temperature_celsius, fan_percentage>
using CurvePoints = QList<CurvePoint>;

struct FanProfile {
    QString name;
    CurvePoints points;
};

class ConfigManager : public QObject
{
    Q_OBJECT
  public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager() override = default;

    bool load(const QString &path = "/etc/lenovo-fanboost.toml");
    void saveAsync(const QString &path = "/etc/lenovo-fanboost.toml");
    bool isBusy() const { return m_busy; }

    QString activeProfile() const;
    void setActiveProfile(const QString &name);

    double interval() const;
    void setInterval(double sec);

    QStringList profileNames() const;
    FanProfile profile(const QString &name) const;
    void addOrUpdateProfile(const QString &name, const CurvePoints &points);
    bool removeProfile(const QString &name);
    bool isDefaultProfile(const QString &name) const;

    static CurvePoints defaultQuietPoints();
    static CurvePoints defaultBalancedPoints();
    static CurvePoints defaultAggressivePoints();

    static QString serializeToToml(const QString &activeProfile, double interval,
                                   const QMap<QString, FanProfile> &profiles);

  signals:
    void configLoaded();
    void activeProfileChanged(const QString &name);
    void saveFinished(bool success, const QString &message, bool serviceRestarted);
    void busyChanged(bool busy);

  private:
    void populateDefaults();
    void setBusy(bool busy);
    void finishSave(bool success, const QString &message, bool serviceRestarted = false);
    void handleSaveProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void startPrivilegedSave();

    QString m_configPath{"/etc/lenovo-fanboost.toml"};
    QString m_activeProfile{"Balanced"};
    double m_interval{0.5};
    QMap<QString, FanProfile> m_profiles;
    QProcess m_saveProcess;
    QTimer m_processTimeout;
    QTemporaryFile m_saveTempFile;
    QString m_savePath;
    QString m_saveContent;
    bool m_busy{false};
    bool m_restartAfterSave{false};
    bool m_processTimedOut{false};
    bool m_loadValid{true};
    bool m_fingerprintInitialized{false};
    bool m_loadedConfigExisted{false};
    QByteArray m_loadedConfigBytes;
};
