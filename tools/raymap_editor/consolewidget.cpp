#include "consolewidget.h"
#include <QColor>

ConsoleWidget::ConsoleWidget(QWidget *parent) : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Toolbar
    QHBoxLayout *toolbar = new QHBoxLayout();
    m_stopButton = new QPushButton(tr("Detener"), this);
    m_stopButton->setIcon(QIcon::fromTheme("process-stop"));
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &ConsoleWidget::stopRequested);
    
    m_clearButton = new QPushButton(tr("Limpiar"), this);
    m_clearButton->setIcon(QIcon::fromTheme("edit-clear"));
    connect(m_clearButton, &QPushButton::clicked, this, &ConsoleWidget::clear);
    
    toolbar->addWidget(m_stopButton);
    toolbar->addWidget(m_clearButton);
    toolbar->addStretch();
    
    layout->addLayout(toolbar);

    // Output area
    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4; font-family: Monospace; font-size: 10pt;");
    layout->addWidget(m_output);
}

void ConsoleWidget::sendText(const QString &text)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    m_output->moveCursor(QTextCursor::End);
}

void ConsoleWidget::appendOutput(const QString &text)
{
    sendText(text);
}

void ConsoleWidget::setBuildMode()
{
    m_stopButton->setEnabled(false);
    sendText("\n--- INICIANDO COMPILACION ---\n");
}

void ConsoleWidget::setRunMode()
{
    m_stopButton->setEnabled(true);
    sendText("\n--- EJECUTANDO ---\n");
}

void ConsoleWidget::clear()
{
    m_output->clear();
}
