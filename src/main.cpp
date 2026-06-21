#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "portalfilepicker.h"
#include "systemtheme.h"

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
    SystemTheme systemTheme(&app);
    backend.setDarkMode(systemTheme.darkMode());
    QObject::connect(&systemTheme, &SystemTheme::darkModeChanged, &backend,
                     &Backend::setDarkMode);

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
