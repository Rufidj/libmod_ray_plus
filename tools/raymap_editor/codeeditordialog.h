#ifndef CODEEDITORDIALOG_H
#define CODEEDITORDIALOG_H

#include <QDialog>
#include "codeeditor.h"

class QPushButton;
class QLabel;

class CodeEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CodeEditorDialog(QWidget *parent = nullptr);
    
    bool openFile(const QString &fileName);
    bool saveFile();
    bool saveFileAs();
    
    QString currentFile() const { return m_editor->currentFile(); }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSave();
    void onSaveAs();
    void onDocumentModified();

private:
    bool maybeSave();
    
    CodeEditor *m_editor;
    QPushButton *m_saveButton;
    QPushButton *m_saveAsButton;
    QPushButton *m_closeButton;
    QLabel *m_statusLabel;
};

#endif // CODEEDITORDIALOG_H
