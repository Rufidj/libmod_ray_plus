#include "buildmanager.h"
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

BuildManager::BuildManager(QObject *parent) 
    : QObject(parent), m_process(new QProcess(this)), m_isrunning(false), m_autoRunAfterBuild(false)
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &BuildManager::onProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &BuildManager::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &BuildManager::onProcessFinished);
            
    detectBennuGD2();
}

void BuildManager::detectBennuGD2()
{
    // Check settings first
    QSettings settings("BennuGD", "RayMapEditor");
    QString customPath = settings.value("bennugdPath").toString();
    
    if (!customPath.isEmpty()) {
        m_bgdcPath = customPath + "/bgdc";
        m_bgdiPath = customPath + "/bgdi";
        if (QFile::exists(m_bgdcPath) && QFile::exists(m_bgdiPath)) return;
    }
    
    // Check standard paths
    QStringList paths = {
        "/usr/local/bin",
        "/usr/bin",
        "/opt/bennugd2/bin",
        QDir::homePath() + "/bennugd2/bin",
        QDir::homePath() + "/.local/bin",
        QDir::currentPath() // Check local directory too
    };
    
    for (const QString &path : paths) {
        if (QFile::exists(path + "/bgdc") && QFile::exists(path + "/bgdi")) {
            m_bgdcPath = path + "/bgdc";
            m_bgdiPath = path + "/bgdi";
            return;
        }
    }
}

bool BuildManager::isBennuGD2Installed()
{
    return !m_bgdcPath.isEmpty() && !m_bgdiPath.isEmpty();
}

void BuildManager::setCustomBennuGDPath(const QString &path)
{
    QSettings settings("BennuGD", "RayMapEditor");
    settings.setValue("bennugdPath", path);
    detectBennuGD2();
}

void BuildManager::buildProject(const QString &projectPath)
{
    if (m_isrunning) return;
    if (m_bgdcPath.isEmpty()) {
        emit executeInTerminal("Error: BennuGD2 compilers not found!\n");
        return;
    }
    
    m_currentProjectPath = projectPath;
    m_autoRunAfterBuild = false; // Just build
    
    QString mainFile = projectPath + "/src/main.prg";
    
    emit buildStarted();
    emit executeInTerminal("Compiling: " + mainFile + "\n");
    
    m_process->setWorkingDirectory(projectPath);
    m_process->start(m_bgdcPath, QStringList() << mainFile);
    m_isrunning = true;
}

void BuildManager::runProject(const QString &projectPath)
{
    if (m_isrunning) return;
    if (m_bgdiPath.isEmpty()) {
        emit executeInTerminal("Error: BennuGD2 interpreter not found!\n");
        return;
    }
    
    m_currentProjectPath = projectPath;
    QString dcbFile = projectPath + "/src/main.dcb";
    
    emit runStarted();
    emit executeInTerminal("Running: " + dcbFile + "\n");
    
    m_process->setWorkingDirectory(projectPath);
    m_process->start(m_bgdiPath, QStringList() << dcbFile);
    m_isrunning = true;
}

void BuildManager::buildAndRunProject(const QString &projectPath)
{
    if (m_isrunning) return;
    m_autoRunAfterBuild = true;
    buildProject(projectPath); // Will trigger run in onProcessFinished
}

void BuildManager::stopRunning()
{
    if (m_isrunning && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        emit executeInTerminal("\nProcess terminated by user.\n");
    }
}

void BuildManager::onProcessOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    emit executeInTerminal(QString::fromLocal8Bit(data));
    data = m_process->readAllStandardError();
    emit executeInTerminal(QString::fromLocal8Bit(data));
}

void BuildManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_isrunning = false;
    
    if (m_autoRunAfterBuild && exitCode == 0 && exitStatus == QProcess::NormalExit) {
        m_autoRunAfterBuild = false; // Clear flag
        emit buildFinished(true);
        runProject(m_currentProjectPath);
    } else if (m_autoRunAfterBuild) {
        // Build failed
        m_autoRunAfterBuild = false;
        emit buildFinished(false);
        emit executeInTerminal("\nBuild Failed. Cannot run.\n");
    } else {
        // Just finished
        emit executeInTerminal(QString("\nProcess finished with exit code %1\n").arg(exitCode));
        emit runFinished();
    }
}
