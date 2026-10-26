#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QMap>

using CurvePoint = QPair<double, double>; // <temperature_celsius, fan_percentage>
using CurvePoints = QList<CurvePoint>;

struct FanProfile {
    QString name;
    CurvePoints points;
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager() override = default;

    bool load(const QString &path = "/etc/lenovo-fanboost.toml");
    bool save(const QString &path = "/etc/lenovo-fanboost.toml", QString *errorMessage = nullptr);

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

private:
    void populateDefaults();

    QString m_configPath{"/etc/lenovo-fanboost.toml"};
    QString m_activeProfile{"Balanced"};
    double m_interval{0.5};
    QMap<QString, FanProfile> m_profiles;
};
