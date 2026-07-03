#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>

class PortalFilePicker : public QObject {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

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
    void clearPending();

    QString m_pendingPath;
    Action m_pendingAction = Action::None;
};
