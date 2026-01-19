#ifndef PROJECTMANAGER_H
#define PROJECTMANAGER_H

#include <QObject>
#include <QString>
#include <QVector>

struct Project {
    QString name;
    QString path;
    // Add other project fields as needed
};

class ProjectManager : public QObject
{
    Q_OBJECT
public:
    explicit ProjectManager(QObject *parent = nullptr);
    
    bool createProject(const QString &path, const QString &name);
    bool openProject(const QString &fileName);
    void closeProject();
    bool hasProject() const;
    const Project* getProject() const;
    QString getProjectPath() const;

private:
    Project *m_project;
};

#endif // PROJECTMANAGER_H
