#include "systemtheme.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QProcess>
#include <QStyleHints>
#include <QVariant>

namespace {
QVariant unwrapVariant(QVariant value) {
    while (value.canConvert<QDBusVariant>())
        value = value.value<QDBusVariant>().variant();
    return value;
}

bool colorSchemeIsDark(const QVariant &value, bool *known) {
    bool ok = false;
    const uint scheme = unwrapVariant(value).toUInt(&ok);
    if (!ok)
        return false;

    if (scheme == 1) {
        *known = true;
        return true;
    }
    if (scheme == 2) {
        *known = true;
        return false;
    }

    return false;
}

bool gsettingsSchemeIsDark(const QVariant &value, bool *known) {
    const QString scheme = unwrapVariant(value).toString();
    if (scheme.contains(QStringLiteral("prefer-dark"))) {
        *known = true;
        return true;
    }
    if (scheme.contains(QStringLiteral("prefer-light"))) {
        *known = true;
        return false;
    }

    return false;
}
}

SystemTheme::SystemTheme(QObject *parent) : QObject(parent) {
    m_darkMode = detectDarkMode();

    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                this, &SystemTheme::refresh);
    }

    QDBusConnection::sessionBus().connect(
        QString(),
        QStringLiteral("/org/freedesktop/portal/desktop"),
        QStringLiteral("org.freedesktop.portal.Settings"),
        QStringLiteral("SettingChanged"),
        this,
        SLOT(handlePortalSettingChanged(QString,QString,QDBusVariant)));
}

void SystemTheme::refresh() {
    setDarkMode(detectDarkMode());
}

void SystemTheme::handlePortalSettingChanged(const QString &nameSpace, const QString &key,
                                             const QDBusVariant &value) {
    if (key != QStringLiteral("color-scheme"))
        return;

    bool known = false;
    bool dark = false;
    if (nameSpace == QStringLiteral("org.freedesktop.appearance"))
        dark = colorSchemeIsDark(value.variant(), &known);
    else if (nameSpace == QStringLiteral("org.gnome.desktop.interface"))
        dark = gsettingsSchemeIsDark(value.variant(), &known);
    else
        return;

    if (known)
        setDarkMode(dark);
    else
        refresh();
}

bool SystemTheme::detectDarkMode() const {
    bool known = false;

    // Prefer the non-blocking Qt color-scheme hint first, as it is instantaneous
    // and does not require IPC or subprocesses.
    const bool qtDark = qtDarkMode(&known);
    if (known)
        return qtDark;

    // Next, query the D-Bus desktop appearance portal settings.
    const bool portalDark = portalDarkMode(&known);
    if (known)
        return portalDark;

    // We avoid falling back to the blocking gsettings QProcess subprocess call on
    // the main GUI thread, as it can block the event loop and freeze the UI.
    return true;
}

bool SystemTheme::portalDarkMode(bool *known) const {
    *known = false;

    QDBusInterface settings(QStringLiteral("org.freedesktop.portal.Desktop"),
                            QStringLiteral("/org/freedesktop/portal/desktop"),
                            QStringLiteral("org.freedesktop.portal.Settings"));
    if (!settings.isValid())
        return false;

    settings.setTimeout(150); // Prevent GUI thread freeze if portal service hangs

    QDBusReply<QDBusVariant> reply = settings.call(
        QStringLiteral("Read"),
        QStringLiteral("org.freedesktop.appearance"),
        QStringLiteral("color-scheme"));
    if (!reply.isValid())
        return false;

    return colorSchemeIsDark(reply.value().variant(), known);
}

bool SystemTheme::gsettingsDarkMode(bool *known) const {
    *known = false;

    QProcess gsettings;
    gsettings.start(QStringLiteral("gsettings"), {
        QStringLiteral("get"),
        QStringLiteral("org.gnome.desktop.interface"),
        QStringLiteral("color-scheme")
    });

    if (!gsettings.waitForFinished(150)) {
        gsettings.kill();
        gsettings.waitForFinished(50);
        return false;
    }

    const QString output = QString::fromUtf8(gsettings.readAllStandardOutput()).trimmed();
    if (output.contains(QStringLiteral("prefer-dark"))) {
        *known = true;
        return true;
    }
    if (output.contains(QStringLiteral("prefer-light"))) {
        *known = true;
        return false;
    }

    return false;
}

bool SystemTheme::qtDarkMode(bool *known) const {
    *known = false;

    if (!QGuiApplication::styleHints())
        return false;

    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark) {
        *known = true;
        return true;
    }
    if (scheme == Qt::ColorScheme::Light) {
        *known = true;
        return false;
    }

    return false;
}

void SystemTheme::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    emit darkModeChanged(m_darkMode);
}
