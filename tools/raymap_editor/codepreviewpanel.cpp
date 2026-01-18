#include "codepreviewpanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QFont>

CodePreviewPanel::CodePreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    // File label
    m_fileLabel = new QLabel(this);
    m_fileLabel->setStyleSheet("QLabel { font-weight: bold; padding: 5px; }");
    layout->addWidget(m_fileLabel);
    
    // Preview (read-only)
    m_preview = new QPlainTextEdit(this);
    m_preview->setReadOnly(true);
    
    // Monospace font
    QFont font("Monospace", 9);
    font.setStyleHint(QFont::TypeWriter);
    m_preview->setFont(font);
    
    layout->addWidget(m_preview);
    
    // Open button
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_openButton = new QPushButton(QIcon::fromTheme("document-open"), "Abrir en Editor", this);
    m_openButton->setEnabled(false);
    connect(m_openButton, &QPushButton::clicked, this, &CodePreviewPanel::onOpenInEditor);
    
    buttonLayout->addWidget(m_openButton);
    layout->addLayout(buttonLayout);
    
    clear();
}

void CodePreviewPanel::showFile(const QString &filePath)
{
    m_currentFile = filePath;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_preview->setPlainText(QString("Error: No se pudo abrir el archivo\n%1").arg(filePath));
        m_fileLabel->setText("Error");
        m_openButton->setEnabled(false);
        return;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    m_preview->setPlainText(content);
    
    QFileInfo fi(filePath);
    m_fileLabel->setText(QString("📄 %1").arg(fi.fileName()));
    m_openButton->setEnabled(true);
}

void CodePreviewPanel::clear()
{
    m_preview->setPlainText("Selecciona un archivo .prg en el explorador de assets para ver su contenido aquí.");
    m_fileLabel->setText("Preview de Código");
    m_openButton->setEnabled(false);
    m_currentFile.clear();
}

void CodePreviewPanel::onOpenInEditor()
{
    if (!m_currentFile.isEmpty()) {
        emit openInEditorRequested(m_currentFile);
    }
}
