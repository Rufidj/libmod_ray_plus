#include "projectmanager.h"
#include "codegenerator.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>

ProjectManager::ProjectManager(QObject *parent) : QObject(parent), m_project(nullptr)
{
}

bool ProjectManager::createProject(const QString &path, const QString &name)
{
    if (m_project) delete m_project;
    m_project = new Project();
    m_project->name = name;
    m_project->path = path;
    
    // Create directory if it doesn't exist
    QDir dir(path);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Create src directory
    dir.mkpath("src");
    dir.mkpath("assets");
    
    // Generate default main.prg
    CodeGenerator generator;
    // We can set project info if needed, but for now default template is fine
    // generator.setProject(m_project); 
    
    QString mainCode = generator.generateMainPrg();
    QFile srcFile(path + "/src/main.prg");
    if (srcFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        srcFile.write(mainCode.toUtf8());
        srcFile.close();
    }
    
    // Save project file
    QJsonObject obj;
    obj["name"] = name;
    obj["version"] = "1.0";
    
    QJsonDocument doc(obj);
    QFile file(path + "/" + name + ".bgd2proj");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        // Also ensure createProject implies opening it? 
        // Logic usually separates creation from opening, but we can set the current logic to "open" it right here by setting m_project values correctly which we did.
        // m_project->name and m_project->path are set.
        return true;
    }
    return false;
}

bool ProjectManager::openProject(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return false;
    
    if (m_project) delete m_project;
    m_project = new Project();
    
    QJsonObject obj = doc.object();
    m_project->name = obj["name"].toString();
    m_project->path = QFileInfo(fileName).path();
    
    return true;
}

void ProjectManager::closeProject()
{
    if (m_project) {
        delete m_project;
        m_project = nullptr;
    }
}

bool ProjectManager::hasProject() const
{
    return m_project != nullptr;
}

const Project* ProjectManager::getProject() const
{
    return m_project;
}

QString ProjectManager::getProjectPath() const
{
    if (m_project) return m_project->path;
    return QString();
}
