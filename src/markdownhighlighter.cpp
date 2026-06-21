#include "markdownhighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextDocument>

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document) {
    rebuildFormats();
}

void MarkdownHighlighter::setDarkMode(bool darkMode) {
    if (m_darkMode == darkMode)
        return;

    m_darkMode = darkMode;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::rebuildFormats() {
    const QColor marker = m_darkMode ? QColor(QStringLiteral("#54565c"))
                                     : QColor(QStringLiteral("#aaa49d"));
    const QColor background = m_darkMode ? QColor(QStringLiteral("#101010"))
                                         : QColor(QStringLiteral("#fbfaf5"));
    const QColor text = m_darkMode ? QColor(QStringLiteral("#d2d2d0"))
                                   : QColor(QStringLiteral("#1e1f21"));
    const QColor link = m_darkMode ? QColor(QStringLiteral("#80a9d8"))
                                   : QColor(QStringLiteral("#315f9b"));
    const QColor quote = m_darkMode ? QColor(QStringLiteral("#88a098"))
                                    : QColor(QStringLiteral("#5f766f"));
    const QColor codeBackground = m_darkMode ? QColor(QStringLiteral("#1d1d1d"))
                                             : QColor(QStringLiteral("#ece8df"));

    m_markerFormat = QTextCharFormat();
    m_markerFormat.setForeground(marker);

    m_hiddenMarkerFormat = QTextCharFormat();
    m_hiddenMarkerFormat.setForeground(background);
    m_hiddenMarkerFormat.setFontPointSize(0.1);
    m_hiddenMarkerFormat.setFontStretch(1);

    m_headingFormat = QTextCharFormat();
    m_headingFormat.setForeground(text);
    m_headingFormat.setFontWeight(QFont::Bold);

    m_boldFormat = QTextCharFormat();
    m_boldFormat.setFontWeight(QFont::Bold);
    m_boldFormat.setForeground(text);

    m_italicFormat = QTextCharFormat();
    m_italicFormat.setFontItalic(true);
    m_italicFormat.setForeground(text);

    m_codeFormat = QTextCharFormat();
    m_codeFormat.setForeground(text);
    m_codeFormat.setBackground(codeBackground);

    m_quoteFormat = QTextCharFormat();
    m_quoteFormat.setForeground(quote);
    m_quoteFormat.setFontItalic(true);

    m_linkFormat = QTextCharFormat();
    m_linkFormat.setForeground(link);
    m_linkFormat.setFontUnderline(true);
}

void MarkdownHighlighter::highlightBlock(const QString &text) {
    highlightMarkers(text);
    highlightInline(text);
}

void MarkdownHighlighter::highlightMarkers(const QString &text) {
    static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})(\\s+)(.*)$"));
    const QRegularExpressionMatch heading = headingRe.match(text);
    if (heading.hasMatch()) {
        setFormat(0, heading.capturedLength(1) + heading.capturedLength(2),
                  m_markerFormat);
        setFormat(heading.capturedStart(3), heading.capturedLength(3),
                  m_headingFormat);
        return;
    }

    static const QRegularExpression quoteRe(QStringLiteral("^(\\s*>+\\s?)(.*)$"));
    const QRegularExpressionMatch quote = quoteRe.match(text);
    if (quote.hasMatch()) {
        setFormat(0, quote.capturedLength(1), m_markerFormat);
        setFormat(quote.capturedStart(2), quote.capturedLength(2), m_quoteFormat);
    }

    static const QRegularExpression listRe(
        QStringLiteral("^(\\s*(?:[-+*]|\\d+[.)])\\s+)(.*)$"));
    const QRegularExpressionMatch list = listRe.match(text);
    if (list.hasMatch())
        setFormat(0, list.capturedLength(1), m_markerFormat);

    static const QRegularExpression ruleRe(QStringLiteral("^\\s{0,3}([-*_])(?:\\s*\\1){2,}\\s*$"));
    const QRegularExpressionMatch rule = ruleRe.match(text);
    if (rule.hasMatch())
        setFormat(0, text.length(), m_markerFormat);
}

void MarkdownHighlighter::highlightInline(const QString &text) {
    static const QRegularExpression codeRe(QStringLiteral("`([^`]+)`"));
    QRegularExpressionMatchIterator codeMatches = codeRe.globalMatch(text);
    while (codeMatches.hasNext()) {
        const QRegularExpressionMatch match = codeMatches.next();
        setFormat(match.capturedStart(0), match.capturedLength(0), m_codeFormat);
    }

    static const QRegularExpression boldRe(QStringLiteral("(\\*\\*|__)(.+?)(\\1)"));
    QRegularExpressionMatchIterator boldMatches = boldRe.globalMatch(text);
    while (boldMatches.hasNext()) {
        const QRegularExpressionMatch match = boldMatches.next();
        setFormat(match.capturedStart(1), match.capturedLength(1), m_hiddenMarkerFormat);
        setFormat(match.capturedStart(2), match.capturedLength(2), m_boldFormat);
        setFormat(match.capturedStart(3), match.capturedLength(3), m_hiddenMarkerFormat);
    }

    static const QRegularExpression italicRe(
        QStringLiteral("(?<!\\*)\\*([^*\\n]+)\\*(?!\\*)|(?<!_)_([^_\\n]+)_(?!_)"));
    QRegularExpressionMatchIterator italicMatches = italicRe.globalMatch(text);
    while (italicMatches.hasNext()) {
        const QRegularExpressionMatch match = italicMatches.next();
        const int contentIndex = match.capturedStart(1) >= 0 ? 1 : 2;
        setFormat(match.capturedStart(0), 1, m_hiddenMarkerFormat);
        setFormat(match.capturedStart(contentIndex), match.capturedLength(contentIndex),
                  m_italicFormat);
        setFormat(match.capturedStart(0) + match.capturedLength(0) - 1, 1,
                  m_hiddenMarkerFormat);
    }

    static const QRegularExpression linkRe(
        QStringLiteral("\\[([^\\]]+)\\]\\(((?:\\\\.|[^)])+)\\)"));
    QRegularExpressionMatchIterator linkMatches = linkRe.globalMatch(text);
    while (linkMatches.hasNext()) {
        const QRegularExpressionMatch match = linkMatches.next();
        setFormat(match.capturedStart(0), 1, m_hiddenMarkerFormat);
        setFormat(match.capturedStart(1), match.capturedLength(1), m_linkFormat);
        setFormat(match.capturedStart(1) + match.capturedLength(1), 2,
                  m_hiddenMarkerFormat);
        setFormat(match.capturedStart(2), match.capturedLength(2), m_hiddenMarkerFormat);
        setFormat(match.capturedStart(0) + match.capturedLength(0) - 1, 1,
                  m_hiddenMarkerFormat);
    }
}
