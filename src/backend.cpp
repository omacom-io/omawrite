#include "backend.h"

#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QProcess>
#include <QQuickTextDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QUrl>

#include "filepicker.h"
#include "markdownhighlighter.h"

namespace {
constexpr qreal typoraLineHeightPercent = 140;

QString normalizedLinkUrl(const QString &clipboardText) {
    QString candidate = clipboardText.trimmed();
    const int newline = candidate.indexOf(QLatin1Char('\n'));
    const int carriageReturn = candidate.indexOf(QLatin1Char('\r'));
    int lineBreak = newline;
    if (lineBreak < 0 || (carriageReturn >= 0 && carriageReturn < lineBreak))
        lineBreak = carriageReturn;
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
    loadOmarchyTheme();
    m_wordCountTimer.setSingleShot(true);
    m_wordCountTimer.setInterval(120);
    connect(&m_wordCountTimer, &QTimer::timeout, this, &Backend::refreshWordCount);

    // Watch the active theme file and directory paths for changes so we reload live
    const QString currentDir = QDir::homePath() + QStringLiteral("/.config/omarchy/current");
    const QString themeDir = currentDir + QStringLiteral("/theme");
    const QString colorsPath = themeDir + QStringLiteral("/colors.toml");

    auto updateWatcherPaths = [this, currentDir, themeDir, colorsPath]() {
        QStringList paths = m_themeWatcher.files() + m_themeWatcher.directories();
        if (!paths.isEmpty())
            m_themeWatcher.removePaths(paths);

        if (QDir(currentDir).exists())
            m_themeWatcher.addPath(currentDir);
        if (QDir(themeDir).exists())
            m_themeWatcher.addPath(themeDir);
        if (QFile::exists(colorsPath))
            m_themeWatcher.addPath(colorsPath);
    };

    updateWatcherPaths();

    // Connect file/directory modifications to reload the color theme dynamically
    auto reloadCallback = [this, updateWatcherPaths]() {
        loadOmarchyTheme();
        if (m_highlighter)
            m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);

        updateWatcherPaths();
    };

    connect(&m_themeWatcher, &QFileSystemWatcher::fileChanged, this, reloadCallback);
    connect(&m_themeWatcher, &QFileSystemWatcher::directoryChanged, this, reloadCallback);

    if (!m_filePicker)
        return;

    connect(m_filePicker, &FilePicker::openSelected, this, &Backend::open);
    connect(m_filePicker, &FilePicker::saveSelected, this, &Backend::saveTo);
    connect(m_filePicker, &FilePicker::canceled, this, [this]() {
        m_closeAfterSave = false;
    });
    connect(m_filePicker, &FilePicker::failed, this, [this](const QString &message) {
        m_closeAfterSave = false;
        setStatus(message);
    });
}

void Backend::setDocumentText(const QString &text) {
    if (m_documentText == text && (!m_document || m_document->toPlainText() == text))
        return;

    m_documentText = text;
    emit documentTextChanged();
    applyDocumentTypography();

    if (m_loading) {
        if (m_wordCountTimer.isActive())
            m_wordCountTimer.stop();
        setWordCount(countWords(m_documentText));
    }
    else
        scheduleWordCount();

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
    m_darkMode = darkMode;
    loadOmarchyTheme();
    if (m_highlighter) {
        m_highlighter->setDarkMode(m_darkMode);
        m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);
    }
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

    m_document = quickDocument->textDocument();
    m_highlighter = new MarkdownHighlighter(m_document);
    m_highlighter->setDarkMode(m_darkMode);
    m_highlighter->setColors(m_themeBackground, m_themeForeground, m_themeAccent);

    connect(m_document, &QTextDocument::contentsChange, this,
            [this](int position, int, int charsAdded) {
                if (m_formattingTypography || m_loading)
                    return;
                m_lastChangePos = position;
                m_lastChangeAdded = charsAdded;
            });

    applyDocumentTypography();
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

void Backend::saveForClose() {
    if (!m_modified) {
        emit closeAfterSave();
        return;
    }

    m_closeAfterSave = true;
    save();
}

void Backend::saveAsDialog() {
    if (!m_filePicker) {
        m_closeAfterSave = false;
        setStatus(QStringLiteral("No file picker is available."));
        return;
    }

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

void Backend::editorTextChanged() {
    if (m_loading || m_formattingTypography)
        return;

    if (m_document) {
        const int blockCount = m_document->blockCount();
        if (blockCount > m_formattedBlockCount)
            reapplyTypographyToChange();
        m_formattedBlockCount = blockCount;
    }

    scheduleWordCount();
    setModified(true);
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
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Only local files can be saved."));
        return;
    }

    const QString targetName = QFileInfo(url.toLocalFile()).fileName();
    QSaveFile file(url.toLocalFile());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Could not save %1.").arg(targetName));
        return;
    }

    const QString text = currentDocumentText();
    file.write(text.toUtf8());

    // commit() flushes, fsyncs, and atomically renames the temp file into place,
    // returning false (and leaving the original untouched) on any write error.
    if (!file.commit()) {
        m_closeAfterSave = false;
        setStatus(QStringLiteral("Could not write %1.").arg(targetName));
        return;
    }

    const bool shouldClose = m_closeAfterSave;
    m_closeAfterSave = false;
    m_documentText = text;
    setFileUrl(url);
    setModified(false);
    setStatus(QStringLiteral("Saved %1").arg(fileName()));

    if (shouldClose)
        emit closeAfterSave();
}

QUrl Backend::suggestedSaveUrl() const {
    if (m_fileUrl.isLocalFile())
        return m_fileUrl;

    return QUrl::fromLocalFile(QDir::home().filePath(QStringLiteral("Untitled.md")));
}

QString Backend::currentDocumentText() const {
    return m_document ? m_document->toPlainText() : m_documentText;
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

void Backend::setWordCount(int words) {
    if (m_wordCount == words)
        return;

    m_wordCount = words;
    emit wordCountChanged();
}

void Backend::refreshWordCount() {
    if (m_wordCountTimer.isActive())
        m_wordCountTimer.stop();

    setWordCount(countWords(currentDocumentText()));
}

void Backend::scheduleWordCount() {
    m_wordCountTimer.start();
}

void Backend::applyDocumentTypography() {
    if (!m_document)
        return;

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // A full pass is only used for freshly loaded/attached documents, so it is
    // safe to drop undo history here (re-enabling clears the stack anyway).
    const bool undoEnabled = m_document->isUndoRedoEnabled();
    m_document->setUndoRedoEnabled(false);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.select(QTextCursor::Document);
    cursor.mergeBlockFormat(blockFormat);
    m_formattingTypography = false;

    m_document->setUndoRedoEnabled(undoEnabled);

    m_formattedBlockCount = m_document->blockCount();
}

void Backend::reapplyTypographyToChange() {
    if (!m_document)
        return;

    QTextBlockFormat blockFormat;
    blockFormat.setLineHeight(typoraLineHeightPercent, QTextBlockFormat::ProportionalHeight);

    // Format only the block(s) touched by the last edit instead of the whole
    // document, and fold the change into the preceding edit command so a single
    // undo reverts both the text and its formatting.
    const int maxPos = m_document->characterCount() - 1;
    const int start = qBound(0, m_lastChangePos, maxPos);
    const int end = qBound(start, m_lastChangePos + m_lastChangeAdded, maxPos);

    m_formattingTypography = true;
    QTextCursor cursor(m_document);
    cursor.joinPreviousEditBlock();
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.mergeBlockFormat(blockFormat);
    cursor.endEditBlock();
    m_formattingTypography = false;
}

void Backend::loadOmarchyTheme() {
    // Set fallback colors first
    m_themeBackground = m_darkMode ? QStringLiteral("#101010") : QStringLiteral("#ffffff");
    m_themeForeground = m_darkMode ? QStringLiteral("#eeeeee") : QStringLiteral("#222324");
    m_themeAccent = m_darkMode ? QStringLiteral("#5584aa") : QStringLiteral("#2077b2");
    m_themeSelection = m_darkMode ? QStringLiteral("#186a9a") : QStringLiteral("#2077b2");

    const QString themeColorsPath = QDir::homePath() + QStringLiteral("/.config/omarchy/current/theme/colors.toml");
    QFile file(themeColorsPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
                continue;

            QStringList parts = line.split(QLatin1Char('='));
            if (parts.size() < 2)
                continue;

            QString key = parts.at(0).trimmed();
            QString val = parts.at(1).trimmed();
            // Remove enclosing quotes if any
            if ((val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"'))) ||
                (val.startsWith(QLatin1Char('\'')) && val.endsWith(QLatin1Char('\'')))) {
                val = val.mid(1, val.length() - 2);
            }

            if (key == QStringLiteral("background"))
                m_themeBackground = val;
            else if (key == QStringLiteral("foreground"))
                m_themeForeground = val;
            else if (key == QStringLiteral("accent"))
                m_themeAccent = val;
            else if (key == QStringLiteral("selection_background"))
                m_themeSelection = val;
        }
    }

    // Determine whether the theme is dark based on the resolved background color
    QColor bgColor(m_themeBackground);
    if (bgColor.isValid()) {
        double luminance = 0.299 * bgColor.redF() + 0.587 * bgColor.greenF() + 0.114 * bgColor.blueF();
        bool isThemeDark = (luminance < 0.5);
        if (isThemeDark != m_darkMode) {
            m_darkMode = isThemeDark;
            emit darkModeChanged();
            if (m_highlighter) {
                m_highlighter->setDarkMode(m_darkMode);
            }
        }
    }

    emit themeColorsChanged();
}

