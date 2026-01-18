#include "codeeditor.h"
#include <QPainter>
#include <QTextBlock>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

// ============================================================================
// BennuGD Syntax Highlighter
// ============================================================================

BennuGDHighlighter::BennuGDHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    // Keywords
    keywordFormat.setForeground(QColor(86, 156, 214)); // Blue
    keywordFormat.setFontWeight(QFont::Bold);
    QStringList keywordPatterns = {
        "\\bif\\b", "\\belse\\b", "\\bend\\b", "\\bwhile\\b", "\\bloop\\b",
        "\\bfor\\b", "\\bfrom\\b", "\\bto\\b", "\\bstep\\b",
        "\\bswitch\\b", "\\bcase\\b", "\\bdefault\\b",
        "\\breturn\\b", "\\bbreak\\b", "\\bcontinue\\b",
        "\\bprocess\\b", "\\bfunction\\b", "\\bbegin\\b",
        "\\bprivate\\b", "\\bpublic\\b", "\\blocal\\b", "\\bglobal\\b",
        "\\bconst\\b", "\\bimport\\b", "\\binclude\\b",
        "\\band\\b", "\\bor\\b", "\\bnot\\b", "\\bxor\\b"
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // Types
    typeFormat.setForeground(QColor(78, 201, 176)); // Cyan
    QStringList typePatterns = {
        "\\bint\\b", "\\bfloat\\b", "\\bstring\\b", "\\bbyte\\b",
        "\\bword\\b", "\\bdword\\b", "\\bpointer\\b"
    };
    for (const QString &pattern : typePatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = typeFormat;
        highlightingRules.append(rule);
    }

    // Functions (uppercase words followed by parenthesis)
    functionFormat.setForeground(QColor(220, 220, 170)); // Yellow
    rule.pattern = QRegularExpression("\\b[A-Z_][A-Z0-9_]*(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    // Numbers
    numberFormat.setForeground(QColor(181, 206, 168)); // Light green
    rule.pattern = QRegularExpression("\\b[0-9]+\\.?[0-9]*\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);

    // Strings
    stringFormat.setForeground(QColor(206, 145, 120)); // Orange
    rule.pattern = QRegularExpression("\".*\"");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    // Single-line comments
    commentFormat.setForeground(QColor(106, 153, 85)); // Green
    commentFormat.setFontItalic(true);
    rule.pattern = QRegularExpression("//[^\n]*");
    rule.format = commentFormat;
    highlightingRules.append(rule);
}

void BennuGDHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}

// ============================================================================
// Code Editor
// ============================================================================

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    lineNumberArea = new LineNumberArea(this);
    highlighter = new BennuGDHighlighter(document());

    connect(this, &CodeEditor::blockCountChanged, this, &CodeEditor::updateLineNumberAreaWidth);
    connect(this, &CodeEditor::updateRequest, this, &CodeEditor::updateLineNumberArea);
    connect(this, &CodeEditor::cursorPositionChanged, this, &CodeEditor::highlightCurrentLine);

    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    // Set font
    QFont font("Monospace", 10);
    font.setStyleHint(QFont::TypeWriter);
    setFont(font);

    // Tab settings
    setTabStopDistance(40); // 4 spaces
}

int CodeEditor::lineNumberAreaWidth()
{
    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 10 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void CodeEditor::updateLineNumberAreaWidth(int /* newBlockCount */)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void CodeEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea->scroll(0, dy);
    else
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void CodeEditor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    lineNumberArea->setGeometry(QRect(cr.left(), cr.top(), lineNumberAreaWidth(), cr.height()));
}

void CodeEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        QTextEdit::ExtraSelection selection;

        QColor lineColor = QColor(Qt::yellow).lighter(160);

        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    setExtraSelections(extraSelections);
}

void CodeEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    painter.fillRect(event->rect(), QColor(240, 240, 240));

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(Qt::gray);
            painter.drawText(0, top, lineNumberArea->width() - 5, fontMetrics().height(),
                           Qt::AlignRight, number);
        }

        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

bool CodeEditor::loadFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    setPlainText(in.readAll());
    file.close();

    m_currentFile = fileName;
    document()->setModified(false);
    return true;
}

bool CodeEditor::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << toPlainText();
    file.close();

    m_currentFile = fileName;
    document()->setModified(false);
    return true;
}

bool CodeEditor::saveFile()
{
    if (m_currentFile.isEmpty()) {
        return false;
    }
    return saveFile(m_currentFile);
}
