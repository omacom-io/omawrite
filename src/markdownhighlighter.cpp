#include "markdownhighlighter.h"

#include <QColor>
#include <QFont>
#include <QTextDocument>
#include <QTextBlock>
#include <QFontMetricsF>

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

void MarkdownHighlighter::setColors(const QString &background, const QString &foreground, const QString &accent) {
    m_customBg = background;
    m_customFg = foreground;
    m_customAccent = accent;
    rebuildFormats();
    rehighlight();
}

void MarkdownHighlighter::rebuildFormats() {
    const QColor marker = m_darkMode ? QColor(QStringLiteral("#4f525a"))
                                     : QColor(QStringLiteral("#aeb1b5"));
    const QColor background = !m_customBg.isEmpty() ? QColor(m_customBg)
                            : (m_darkMode ? QColor(QStringLiteral("#101010"))
                                          : QColor(QStringLiteral("#ffffff")));
    const QColor text = !m_customFg.isEmpty() ? QColor(m_customFg)
                      : (m_darkMode ? QColor(QStringLiteral("#eeeeee"))
                                    : QColor(QStringLiteral("#222324")));
    const QColor link = !m_customAccent.isEmpty() ? QColor(m_customAccent)
                      : (m_darkMode ? QColor(QStringLiteral("#5584aa"))
                                    : QColor(QStringLiteral("#2077b2")));
    const QColor quote = marker;
    const QColor codeBackground = m_darkMode ? QColor(QStringLiteral("#1c1a1a"))
                                             : QColor(QStringLiteral("#f8f8f8"));

    m_markerFormat = QTextCharFormat();
    m_markerFormat.setForeground(marker);

    m_hiddenMarkerFormat = QTextCharFormat();
    m_hiddenMarkerFormat.setForeground(background);
    m_hiddenMarkerFormat.setFontPointSize(1.0);

    double charWidth = 1.0;
    QFont fallbackFont = m_hiddenMarkerFormat.font();
    fallbackFont.setPointSizeF(1.0);
    QFontMetricsF fmFallback(fallbackFont);
    charWidth = fmFallback.horizontalAdvance(QLatin1Char('['));

    if (document()) {
        QFont font = document()->defaultFont();
        font.setPointSizeF(1.0);
        QFontMetricsF fm(font);
        charWidth = fm.horizontalAdvance(QLatin1Char('['));
    }
    m_hiddenMarkerFormat.setFontLetterSpacingType(QFont::AbsoluteSpacing);
    m_hiddenMarkerFormat.setFontLetterSpacing(-charWidth);

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
    if (text.isEmpty())
        return;

    highlightMarkers(text);
    if (text.contains(QLatin1Char('`')) || text.contains(QLatin1Char('*'))
            || text.contains(QLatin1Char('_')) || text.contains(QLatin1Char('['))) {
        highlightInline(text);
    }
}

void MarkdownHighlighter::highlightMarkers(const QString &text) {
    int first = 0;
    while (first < text.length() && text.at(first).isSpace())
        ++first;
    if (first >= text.length())
        return;

    const QChar firstChar = text.at(first);
    if (first == 0 && firstChar == QLatin1Char('#')) {
        static const QRegularExpression headingRe(QStringLiteral("^(#{1,6})(\\s+)(.*)$"));
        const QRegularExpressionMatch heading = headingRe.match(text);
        if (heading.hasMatch()) {
            setFormat(0, heading.capturedLength(1) + heading.capturedLength(2),
                      m_markerFormat);
            setFormat(heading.capturedStart(3), heading.capturedLength(3),
                      m_headingFormat);
            return;
        }
    }

    if (firstChar == QLatin1Char('>')) {
        static const QRegularExpression quoteRe(QStringLiteral("^(\\s*>+\\s?)(.*)$"));
        const QRegularExpressionMatch quote = quoteRe.match(text);
        if (quote.hasMatch()) {
            setFormat(0, quote.capturedLength(1), m_markerFormat);
            setFormat(quote.capturedStart(2), quote.capturedLength(2), m_quoteFormat);
        }
    }

    if (firstChar == QLatin1Char('-') || firstChar == QLatin1Char('+')
            || firstChar == QLatin1Char('*') || firstChar.isDigit()) {
        static const QRegularExpression listRe(
            QStringLiteral("^(\\s*(?:[-+*]|\\d+[.)])\\s+)(.*)$"));
        const QRegularExpressionMatch list = listRe.match(text);
        if (list.hasMatch())
            setFormat(0, list.capturedLength(1), m_markerFormat);
    }

    if (firstChar == QLatin1Char('-') || firstChar == QLatin1Char('*')
            || firstChar == QLatin1Char('_')) {
        static const QRegularExpression ruleRe(QStringLiteral("^\\s{0,3}([-*_])(?:\\s*\\1){2,}\\s*$"));
        const QRegularExpressionMatch rule = ruleRe.match(text);
        if (rule.hasMatch())
            setFormat(0, text.length(), m_markerFormat);
    }
}

void MarkdownHighlighter::highlightInline(const QString &text) {
    const bool hasBacktick = text.contains(QLatin1Char('`'));
    const bool hasAsterisk = text.contains(QLatin1Char('*'));
    const bool hasUnderscore = text.contains(QLatin1Char('_'));
    const bool hasLink = text.contains(QLatin1Char('['))
        && text.contains(QStringLiteral("]("));

    if (hasBacktick) {
        static const QRegularExpression codeRe(QStringLiteral("`([^`]+)`"));
        QRegularExpressionMatchIterator codeMatches = codeRe.globalMatch(text);
        while (codeMatches.hasNext()) {
            const QRegularExpressionMatch match = codeMatches.next();
            setFormat(match.capturedStart(0), match.capturedLength(0), m_codeFormat);
        }
    }

    if (hasAsterisk || hasUnderscore) {
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
    }

    if (!hasLink)
        return;

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
