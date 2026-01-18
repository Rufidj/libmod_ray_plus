#ifndef CONSOLEWIDGET_H
#define CONSOLEWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>

class ConsoleWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ConsoleWidget(QWidget *parent = nullptr);
    void sendText(const QString &text);
    void setBuildMode();
    void setRunMode();
    void appendOutput(const QString &text); // Added alias for consistency
    void clear();

signals:
    void stopRequested();

private:
    QTextEdit *m_output;
    QPushButton *m_stopButton;
    QPushButton *m_clearButton;
};

#endif // CONSOLEWIDGET_H
