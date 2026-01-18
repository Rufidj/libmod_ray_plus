#ifndef BUILDMANAGER_H
#define BUILDMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

class BuildManager : public QObject
{
    Q_OBJECT
public:
    explicit BuildManager(QObject *parent = nullptr);
    
    void buildProject(const QString &projectPath);
    void runProject(const QString &projectPath);
    void buildAndRunProject(const QString &projectPath);
    void stopRunning();
    
    bool isBennuGD2Installed();
    void detectBennuGD2();
    void setCustomBennuGDPath(const QString &path);

signals:
    void buildStarted();
    void runStarted();
    void executeInTerminal(const QString &text);
    void buildFinished(bool success);
    void runFinished();

private slots:
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;
    QString m_bgdcPath;
    QString m_bgdiPath;
    
    bool m_isrunning;
    bool m_autoRunAfterBuild;
    QString m_currentProjectPath;
};

#endif // BUILDMANAGER_H
