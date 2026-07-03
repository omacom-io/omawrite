#pragma once

#include <QObject>

class QDBusVariant;

class SystemTheme : public QObject {
    Q_OBJECT

public:
    explicit SystemTheme(QObject *parent = nullptr);

    bool darkMode() const { return m_darkMode; }

signals:
    void darkModeChanged(bool darkMode);

public slots:
    void refresh();

private slots:
    void handlePortalSettingChanged(const QString &nameSpace, const QString &key,
                                    const QDBusVariant &value);

private:
    bool detectDarkMode() const;
    bool portalDarkMode(bool *known) const;
    bool qtDarkMode(bool *known) const;
    void setDarkMode(bool darkMode);

    bool m_darkMode = true;
};
