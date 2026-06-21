#include "backend.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <QUrl>

#include "filepicker.h"
#include "markdownhighlighter.h"

namespace {
QString normalizedLinkUrl(const QString &clipboardText) {
    QString candidate = clipboardText.trimmed();
    const int lineBreak = candidate.indexOf(QRegularExpression(QStringLiteral("[\\r\\n]")));
    if (lineBreak >= 0)
        candidate = candidate.left(lineBreak).trimmed();

    if (candidate.isEmpty())
        return {};

    if (candidate.startsWith(QStringLiteral("www."), Qt::CaseInsensitive))
        candidate.prepend(QStringLiteral("https://"));

    static const QRegularExpression schemeRe(
        QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*:"));
    if (!schemeRe.match(candidate).hasMatch())
        return {};

    const QUrl url(candidate);
    if (!url.isValid() || url.scheme().isEmpty())
        return {};

    const QString scheme = url.scheme().toLower();
    const bool webUrl = scheme == QStringLiteral("http")
        || scheme == QStringLiteral("https")
        || scheme == QStringLiteral("ftp");
    if (webUrl && url.host().isEmpty())
        return {};

    if (!webUrl && scheme != QStringLiteral("mailto"))
        return {};

    return url.toString();
}
}

Backend::Backend(FilePicker *filePicker, QObject *parent)
    : QObject(parent), m_filePicker(filePicker) {
    if (!m_filePicker)
        return;

    connect(m_filePicker, &FilePicker::openSelected, this, &Backend::open);
    connect(m_filePicker, &FilePicker::saveSelected, this, &Backend::saveTo);
    connect(m_filePicker, &FilePicker::failed, this, &Backend::setStatus);
}

void Backend::setDocumentText(const QString &text) {
    if (m_documentText == text)
        return;

    m_documentText = text;
    emit documentTextChanged();

    const int words = countWords(m_documentText);
    if (m_wordCount != words) {
        m_wordCount = words;
        emit wordCountChanged();
    }

    if (!m_loading)
        setModified(true);
}

QString Backend::fileName() const {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty())
        return QStringLiteral("Untitled.md");

    if (m_fileUrl.isLocalFile()) {
        const QFileInfo info(m_fileUrl.toLocalFile());
        if (!info.fileName().isEmpty())
            return info.fileName();
    }

    const QString name = m_fileUrl.fileName();
    return name.isEmpty() ? QStringLiteral("Untitled.md") : name;
}

void Backend::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    if (m_highlighter)
        m_highlighter->setDarkMode(m_darkMode);
    emit darkModeChanged();
}

void Backend::attachDocument(QObject *textDocument) {
    auto *quickDocument = qobject_cast<QQuickTextDocument *>(textDocument);
    if (!quickDocument || !quickDocument->textDocument()) {
        setStatus(QStringLiteral("Could not attach the Markdown renderer."));
        return;
    }

    if (m_highlighter)
        delete m_highlighter.data();

    m_highlighter = new MarkdownHighlighter(quickDocument->textDocument());
    m_highlighter->setDarkMode(m_darkMode);
}

void Backend::openDialog() {
    if (m_filePicker)
        m_filePicker->openDocument();
}

void Backend::open(const QUrl &url) {
    if (!url.isLocalFile()) {
        setStatus(QStringLiteral("Only local files can be opened."));
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStatus(QStringLiteral("Could not open %1.").arg(targetName));
        return;
    }

    const QByteArray bytes = file.readAll();
    m_loading = true;
    setDocumentText(QString::fromUtf8(bytes));
    m_loading = false;
    setFileUrl(url);
    setModified(false);
    setStatus(QStringLiteral("Opened %1").arg(fileName()));
}

void Backend::save() {
    if (!m_fileUrl.isValid() || m_fileUrl.isEmpty()) {
        saveAsDialog();
        return;
    }

    saveTo(m_fileUrl);
}

void Backend::saveAsDialog() {
    if (m_filePicker)
        m_filePicker->saveDocument(suggestedSaveUrl());
}

void Backend::newWindow() {
    const bool started = QProcess::startDetached(QCoreApplication::applicationFilePath(),
                                                 QStringList());
    if (!started)
        setStatus(QStringLiteral("Could not open a new window."));
}

QString Backend::clipboardUrl() const {
    const QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return {};

    const QMimeData *mimeData = clipboard->mimeData();
    if (!mimeData)
        return {};

    if (mimeData->hasUrls()) {
        const QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            const QString normalized = normalizedLinkUrl(url.toString());
            if (!normalized.isEmpty())
                return normalized;
        }
    }

    if (!mimeData->hasText())
        return {};

    return normalizedLinkUrl(mimeData->text());
}

void Backend::setFileUrl(const QUrl &url) {
    if (m_fileUrl == url)
        return;

    m_fileUrl = url;
    emit fileUrlChanged();
}

void Backend::setModified(bool modified) {
    if (m_modified == modified)
        return;

    m_modified = modified;
    emit modifiedChanged();
}

void Backend::setStatus(const QString &status) {
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged();
}

void Backend::saveTo(const QUrl &url) {
    if (!url.isLocalFile()) {
        setStatus(QStringLiteral("Only local files can be saved."));
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        setStatus(QStringLiteral("Could not save %1.").arg(targetName));
        return;
    }

    file.write(m_documentText.toUtf8());
    if (file.error() != QFileDevice::NoError) {
        setStatus(QStringLiteral("Could not write %1.").arg(targetName));
        return;
    }

    setFileUrl(url);
    setModified(false);
    setStatus(QStringLiteral("Saved %1").arg(fileName()));
}

QUrl Backend::suggestedSaveUrl() const {
    if (m_fileUrl.isLocalFile())
        return m_fileUrl;

    return QUrl::fromLocalFile(QDir::home().filePath(QStringLiteral("Untitled.md")));
}

int Backend::countWords(const QString &text) const {
    static const QRegularExpression wordRe(
        QStringLiteral("[\\p{L}\\p{N}]+(?:['-][\\p{L}\\p{N}]+)*"));
    int count = 0;
    QRegularExpressionMatchIterator it = wordRe.globalMatch(text);
    while (it.hasNext()) {
        it.next();
        ++count;
    }
    return count;
}
