#include <QFont>
#include <QFontDatabase>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QGuiApplication>
#include <QIcon>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStyleHints>
#include <QUrl>

#include "backend.h"
#include "portalfilepicker.h"

namespace {
void loadWriterFonts() {
    const QString base = QStringLiteral("/usr/share/fonts/ttf-ia-writer/");
    const QStringList files = {
        QStringLiteral("iAWriterMonoS-Regular.ttf"),
        QStringLiteral("iAWriterMonoS-Italic.ttf"),
        QStringLiteral("iAWriterMonoS-Bold.ttf"),
        QStringLiteral("iAWriterMonoS-BoldItalic.ttf")
    };

    for (const QString &file : files)
        QFontDatabase::addApplicationFont(base + file);
}

bool portalDarkMode(bool *known) {
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

    const uint scheme = reply.value().variant().toUInt();
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

bool gsettingsDarkMode(bool *known) {
    *known = false;

    QProcess gsettings;
    gsettings.start(QStringLiteral("gsettings"), {
        QStringLiteral("get"),
        QStringLiteral("org.gnome.desktop.interface"),
        QStringLiteral("color-scheme")
    });

    if (!gsettings.waitForFinished(250))
        return false;

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

bool detectDarkMode() {
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark)
        return true;
    if (scheme == Qt::ColorScheme::Light)
        return false;

    bool known = false;
    const bool portalDark = portalDarkMode(&known);
    if (known)
        return portalDark;

    const bool gsettingsDark = gsettingsDarkMode(&known);
    if (known)
        return gsettingsDark;

    return true;
}
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("omawrite"));
    app.setDesktopFileName(QStringLiteral("omawrite"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("omawrite")));

    loadWriterFonts();
    app.setFont(QFont(QStringLiteral("iA Writer Mono S")));

    QQuickStyle::setStyle(QStringLiteral("Material"));

    PortalFilePicker filePicker;
    Backend backend(&filePicker, &app);
    backend.setDarkMode(detectDarkMode());
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, &backend,
                     [&backend]() {
        backend.setDarkMode(detectDarkMode());
    });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    const QStringList args = app.arguments();
    if (args.size() > 1)
        backend.open(QUrl::fromLocalFile(args.at(1)));

    return app.exec();
}
