#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

// Syntax highlighter for BennuGD
class BennuGDHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    BennuGDHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    QTextCharFormat keywordFormat;
    QTextCharFormat typeFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat functionFormat;
};

// Code editor widget
class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = nullptr);

    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();

    // File operations
    bool loadFile(const QString &fileName);
    bool saveFile(const QString &fileName);
    bool saveFile(); // Save to current file
    
    QString currentFile() const { return m_currentFile; }
    bool isModified() const { return document()->isModified(); }

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &rect, int dy);

private:
    QWidget *lineNumberArea;
    BennuGDHighlighter *highlighter;
    QString m_currentFile;
};

// Line number area widget
class LineNumberArea : public QWidget
{
public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor), codeEditor(editor) {}

    QSize sizeHint() const override {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *codeEditor;
};

#endif // CODEEDITOR_H
