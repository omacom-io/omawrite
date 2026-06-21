#pragma once

#include <QVariantMap>

#include "filepicker.h"

class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);

    void openDocument() override;
    void saveDocument(const QUrl &suggestedUrl) override;

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    enum class Action {
        None,
        Open,
        Save
    };

    bool requestFile(const QString &method, const QString &title,
                     const QVariantMap &options, Action action);
    void clearPending();

    QString m_pendingPath;
    Action m_pendingAction = Action::None;
};
