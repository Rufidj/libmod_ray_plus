#ifndef CODEGENERATOR_H
#define CODEGENERATOR_H

#include <QString>
#include <QMap>
#include <QVector>
#include "projectmanager.h"
#include "mapdata.h"

class CodeGenerator
{
public:
    CodeGenerator();
    
    // Set project data
    void setProjectData(const ProjectData &data);
    
    // Template system
    void setVariable(const QString &name, const QString &value);
    QString processTemplate(const QString &templateText);
    
    // Main generation
    QString generateMainPrg();
    QString generateMainPrgWithEntities(const QVector<EntityInstance> &entities);
    
    // Entity code generation
    QString generateEntityProcess(const QString &entityName, const QString &entityType);
    
private:
    ProjectData m_projectData;
    QMap<QString, QString> m_variables;
    
    // Helper functions
    QString getMainTemplate();
    QString getPlayerTemplate();
    QString getEnemyTemplate();
};

#endif // CODEGENERATOR_H
