#ifndef CODEPREVIEWPANEL_H
#define CODEPREVIEWPANEL_H

#include <QWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>

class CodePreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CodePreviewPanel(QWidget *parent = nullptr);
    
    void showFile(const QString &filePath);
    void clear();

signals:
    void openInEditorRequested(const QString &filePath);

private slots:
    void onOpenInEditor();

private:
    QPlainTextEdit *m_preview;
    QPushButton *m_openButton;
    QLabel *m_fileLabel;
    QString m_currentFile;
};

#endif // CODEPREVIEWPANEL_H
