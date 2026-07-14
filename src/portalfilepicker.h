#pragma once

#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVariantMap>

class QWindow;

class PortalFilePicker : public QObject {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void setParentWindow(QWindow *window);
    void openDocument();
    void saveDocument(const QUrl &suggestedUrl);

signals:
    void openSelected(const QUrl &url);
    void saveSelected(const QUrl &url);
    void canceled();
    void failed(const QString &message);

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    enum class Action {
        None,
        Open,
        Save
    };

    bool requestFile(const QString &method, const QString &title,
                     QVariantMap options, Action action);
    bool sendPendingRequest();
    QString parentWindowIdentifier() const;
    void clearPending();

    QPointer<QWindow> m_parentWindow;
    QString m_parentWindowHandle;
    bool m_parentExportPending = false;
    QString m_pendingMethod;
    QString m_pendingTitle;
    QVariantMap m_pendingOptions;
    QString m_pendingPath;
    Action m_pendingAction = Action::None;
};
