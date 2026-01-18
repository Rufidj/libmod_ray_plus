#include "codeeditordialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QCloseEvent>

CodeEditorDialog::CodeEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Editor de Código");
    resize(800, 600);
    
    // Create editor
    m_editor = new CodeEditor(this);
    
    // Create buttons
    m_saveButton = new QPushButton(QIcon::fromTheme("document-save"), "Guardar", this);
    m_saveButton->setShortcut(QKeySequence::Save);
    connect(m_saveButton, &QPushButton::clicked, this, &CodeEditorDialog::onSave);
    
    m_saveAsButton = new QPushButton(QIcon::fromTheme("document-save-as"), "Guardar Como...", this);
    m_saveAsButton->setShortcut(QKeySequence::SaveAs);
    connect(m_saveAsButton, &QPushButton::clicked, this, &CodeEditorDialog::onSaveAs);
    
    m_closeButton = new QPushButton("Cerrar", this);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);
    
    m_statusLabel = new QLabel(this);
    
    // Layout
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_editor);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(m_statusLabel);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_saveAsButton);
    buttonLayout->addWidget(m_closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Connect signals
    connect(m_editor->document(), &QTextDocument::modificationChanged,
            this, &CodeEditorDialog::onDocumentModified);
}

bool CodeEditorDialog::openFile(const QString &fileName)
{
    if (!m_editor->loadFile(fileName)) {
        QMessageBox::warning(this, "Error",
            QString("No se pudo abrir el archivo:\n%1").arg(fileName));
        return false;
    }
    
    QFileInfo fi(fileName);
    setWindowTitle(QString("Editor de Código - %1").arg(fi.fileName()));
    m_statusLabel->setText(fileName);
    
    return true;
}

bool CodeEditorDialog::saveFile()
{
    if (m_editor->currentFile().isEmpty()) {
        return saveFileAs();
    }
    
    if (!m_editor->saveFile()) {
        QMessageBox::warning(this, "Error",
            QString("No se pudo guardar el archivo:\n%1").arg(m_editor->currentFile()));
        return false;
    }
    
    m_statusLabel->setText(QString("Guardado: %1").arg(m_editor->currentFile()));
    return true;
}

bool CodeEditorDialog::saveFileAs()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        "Guardar Código",
        m_editor->currentFile().isEmpty() ? "" : m_editor->currentFile(),
        "Archivos BennuGD (*.prg);;Todos los archivos (*)"
    );
    
    if (fileName.isEmpty()) {
        return false;
    }
    
    if (!m_editor->saveFile(fileName)) {
        QMessageBox::warning(this, "Error",
            QString("No se pudo guardar el archivo:\n%1").arg(fileName));
        return false;
    }
    
    QFileInfo fi(fileName);
    setWindowTitle(QString("Editor de Código - %1").arg(fi.fileName()));
    m_statusLabel->setText(QString("Guardado: %1").arg(fileName));
    
    return true;
}

void CodeEditorDialog::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void CodeEditorDialog::onSave()
{
    saveFile();
}

void CodeEditorDialog::onSaveAs()
{
    saveFileAs();
}

void CodeEditorDialog::onDocumentModified()
{
    if (m_editor->isModified()) {
        setWindowTitle(windowTitle() + " *");
    }
}

bool CodeEditorDialog::maybeSave()
{
    if (!m_editor->isModified()) {
        return true;
    }
    
    QMessageBox::StandardButton ret = QMessageBox::warning(
        this,
        "Código Modificado",
        "El código ha sido modificado.\n¿Deseas guardar los cambios?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel
    );
    
    if (ret == QMessageBox::Save) {
        return saveFile();
    } else if (ret == QMessageBox::Cancel) {
        return false;
    }
    
    return true;
}
