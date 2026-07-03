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

// The portal creates the Request object at a path derived from our unique bus
// name and the handle_token we pass, so we can predict it and subscribe before
// the call returns. This avoids missing a Response emitted before we connect.
QString expectedRequestPath(const QString &token) {
    QString sender = QDBusConnection::sessionBus().baseService();
    if (sender.startsWith(QLatin1Char(':')))
        sender.remove(0, 1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2")
        .arg(sender, token);
}

QByteArray portalPathBytes(const QString &path) {
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}
}

PortalFilePicker::PortalFilePicker(QObject *parent) : QObject(parent) {}

void PortalFilePicker::openDocument() {
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), QStringLiteral("Open"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(QDir::homePath()));

    requestFile(QStringLiteral("OpenFile"), QStringLiteral("Open File"),
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
    options.insert(QStringLiteral("accept_label"), QStringLiteral("Save"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(folder));
    options.insert(QStringLiteral("current_name"), name);

    requestFile(QStringLiteral("SaveFile"), QStringLiteral("Save File"),
                options, Action::Save);
}

bool PortalFilePicker::requestFile(const QString &method, const QString &title,
                                   QVariantMap options, Action action) {
    if (m_pendingAction != Action::None) {
        emit failed(QStringLiteral("A file picker is already open."));
        return false;
    }

    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"),
                          QDBusConnection::sessionBus());
    if (!portal.isValid()) {
        emit failed(QStringLiteral("The XDG desktop portal file chooser is not available."));
        return false;
    }

    const QString token = portalToken();
    options.insert(QStringLiteral("handle_token"), token);

    // Subscribe to the predicted Response path *before* issuing the call so a
    // fast backend cannot emit Response before we are listening.
    m_pendingPath = expectedRequestPath(token);
    m_pendingAction = action;
    const bool connected = QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
        this, SLOT(handleResponse(uint,QVariantMap)));
    if (!connected) {
        clearPending();
        emit failed(QStringLiteral("Could not listen for the portal file picker response."));
        return false;
    }

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

        // Older portals may hand back a different path than the predicted one;
        // re-subscribe to the authoritative path if so.
        const QString actualPath = reply.value().path();
        if (actualPath != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(handleResponse(uint,QVariantMap)));
            m_pendingPath = actualPath;
            const bool connected = QDBusConnection::sessionBus().connect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(handleResponse(uint,QVariantMap)));
            if (!connected) {
                clearPending();
                emit failed(QStringLiteral("Could not listen for the portal file picker response."));
            }
        }
    });

    return true;
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    const Action action = m_pendingAction;
    clearPending();

    if (response != 0) {
        emit canceled();
        return;
    }

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (uris.isEmpty()) {
        emit canceled();
        return;
    }

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
