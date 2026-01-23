#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <QString>
#include <QObject>
#include "projectmanager.h"

class Publisher : public QObject
{
    Q_OBJECT
public:
    explicit Publisher(QObject *parent = nullptr);

    enum Platform {
        Linux,
        Android,
        Windows,
        MacOS,
        Switch,
        Web
    };

    struct PublishConfig {
        Platform platform;
        QString outputPath;
        
        // Linux
        bool generateAppImage = false;
        bool generateLinuxStandalone = false;
        bool generateLinuxArchive = true;
        QString appImageToolPath;
        
        // Windows
        bool generateStandalone = false;
        bool generateSfx = false;
        bool generateZip = true;
        
        // Switch
        QString switchAuthor;

        // Android
        QString packageName;
        QString iconPath;
        bool fullProject = true;    // For Android: Generate full android studio project
        bool generateAPK = false;
        QString ndkPath; // Optional override
        
        // Web
        QString emsdkPath; // Path to EMSDK root
        QString webTitle;
    };

    bool publish(const ProjectData &project, const PublishConfig &config);

signals:
    void progress(int percentage, QString message);
    void finished(bool success, QString message);

private:
    bool publishLinux(const ProjectData &project, const PublishConfig &config);
    bool publishAndroid(const ProjectData &project, const PublishConfig &config);
    bool publishWindows(const ProjectData &project, const PublishConfig &config);
    bool publishSwitch(const ProjectData &project, const PublishConfig &config);
    bool publishWeb(const ProjectData &project, const PublishConfig &config);
    
    // Helper
    bool copyDir(const QString &source, const QString &destination);
};

#endif // PUBLISHER_H
