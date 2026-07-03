#include "systemtheme.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QGuiApplication>
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

    const bool portalDark = portalDarkMode(&known);
    if (known)
        return portalDark;

    const bool qtDark = qtDarkMode(&known);
    if (known)
        return qtDark;

    return true;
}

bool SystemTheme::portalDarkMode(bool *known) const {
    *known = false;

    QDBusInterface settings(QStringLiteral("org.freedesktop.portal.Desktop"),
                            QStringLiteral("/org/freedesktop/portal/desktop"),
                            QStringLiteral("org.freedesktop.portal.Settings"));
    if (!settings.isValid())
        return false;

    QDBusReply<QDBusVariant> reply = settings.call(
        QStringLiteral("Read"),
        QStringLiteral("org.freedesktop.appearance"),
        QStringLiteral("color-scheme"));
    if (!reply.isValid())
        return false;

    return colorSchemeIsDark(reply.value().variant(), known);
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
