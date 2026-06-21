#include "portalfilepicker.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>

namespace {
QString portalToken() {
    return QStringLiteral("omawrite_%1").arg(QRandomGenerator::global()->generate());
}

QByteArray portalPathBytes(const QString &path) {
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}
}

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {}

void PortalFilePicker::openDocument() {
    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), portalToken());
    options.insert(QStringLiteral("accept_label"), QStringLiteral("Open"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(QDir::homePath()));

    requestFile(QStringLiteral("OpenFile"), QStringLiteral("Open Markdown file"),
                options, Action::Open);
}

void PortalFilePicker::saveDocument(const QUrl &suggestedUrl) {
    const QString suggestedPath = suggestedUrl.isLocalFile()
        ? suggestedUrl.toLocalFile()
        : QDir::home().filePath(QStringLiteral("Untitled.md"));
    const QFileInfo target(suggestedPath);
    const QString folder = target.absolutePath().isEmpty()
        ? QDir::homePath()
        : target.absolutePath();
    const QString name = target.fileName().isEmpty()
        ? QStringLiteral("Untitled.md")
        : target.fileName();

    QVariantMap options;
    options.insert(QStringLiteral("handle_token"), portalToken());
    options.insert(QStringLiteral("accept_label"), QStringLiteral("Save"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(folder));
    options.insert(QStringLiteral("current_name"), name);

    requestFile(QStringLiteral("SaveFile"), QStringLiteral("Save Markdown file"),
                options, Action::Save);
}

bool PortalFilePicker::requestFile(const QString &method, const QString &title,
                                   const QVariantMap &options, Action action) {
    if (m_pendingAction != Action::None)
        return false;

    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"),
                          QDBusConnection::sessionBus());
    if (!portal.isValid()) {
        emit failed(QStringLiteral("The XDG desktop portal file chooser is not available."));
        return false;
    }

    m_pendingAction = action;
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(method, QString(), title, options), this);

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();

        if (reply.isError()) {
            clearPending();
            emit failed(QStringLiteral("The portal file picker failed: %1")
                            .arg(reply.error().message()));
            return;
        }

        m_pendingPath = reply.value().path();
        const bool connected = QDBusConnection::sessionBus().connect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(handleResponse(uint,QVariantMap)));
        if (!connected) {
            clearPending();
            emit failed(QStringLiteral("Could not listen for the portal file picker response."));
        }
    });

    return true;
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    const Action action = m_pendingAction;
    clearPending();

    if (response != 0)
        return;

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (uris.isEmpty())
        return;

    const QUrl url(uris.first());
    if (action == Action::Open)
        emit openSelected(url);
    else if (action == Action::Save)
        emit saveSelected(url);
}

void PortalFilePicker::clearPending() {
    if (!m_pendingPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(handleResponse(uint,QVariantMap)));
    }

    m_pendingPath.clear();
    m_pendingAction = Action::None;
}
