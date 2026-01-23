#include "publisher.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication>
#include <QProcess>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>
#include <QDirIterator>
#include <QImage>
#include <QPainter>
#include <QColor>

Publisher::Publisher(QObject *parent) : QObject(parent)
{
}

bool Publisher::publish(const ProjectData &project, const PublishConfig &config)
{
    emit progress(0, "Iniciando publicación...");
    
    // Ensure output directory exists (parent of final output)
    QDir outputDir(config.outputPath);
    if (!outputDir.exists()) {
        outputDir.mkpath(".");
    }
    
    bool success = false;
    
    switch (config.platform) {
    case Linux:
        success = publishLinux(project, config);
        break;
    case Android:
        success = publishAndroid(project, config);
        break;
    case Windows:
        success = publishWindows(project, config);
        break;
    case Switch:
        success = publishSwitch(project, config);
        break;
    case Web:
        success = publishWeb(project, config);
        break;
    default:
        emit finished(false, "Plataforma no soportada aún.");
        return false;
    }
    
    if (success) {
        emit finished(true, "Publicación completada exitosamente.");
    } else {
        // finished handled inside specific methods or generic fail
        // If specific method didn't emit finished, emit here?
        // Let's assume specific methods return false on error and emit finished(false, reason) internally?
        // No, better design: return success/fail and emit finished HERE.
    }
    return success;
}

bool Publisher::publishLinux(const ProjectData &project, const PublishConfig &config)
{
    emit progress(10, "Preparando entorno Linux...");
    
    QString baseName = project.name.simplified().replace(" ", "_");
    QString distDir = config.outputPath + "/" + baseName;
    
    // Clean previous output
    QDir dir(distDir);
    if (dir.exists()) dir.removeRecursively();
    dir.mkpath(".");
    
    QDir libDir(distDir + "/libs");
    libDir.mkpath(".");
    
    QDir assetsDir(distDir + "/assets");
    assetsDir.mkpath(".");
    
    // Find runtime directory (for bgdi and libs)
    QString appDir = QCoreApplication::applicationDirPath();
    QDir searchDir(appDir);
    QString runtimeDir;
    
    for (int i = 0; i < 4; i++) {
        QString candidate = searchDir.absoluteFilePath("runtime/linux-gnu");
        if (QDir(candidate).exists()) {
            runtimeDir = candidate;
            qDebug() << "Found runtime dir:" << runtimeDir;
            break;
        }
        searchDir.cdUp();
    }
    
    // 1. Copy Compiled Game (.dcb)
    emit progress(20, "Buscando binario compilado...");
    
    // Determine source DCB path based on mainScript
    QFileInfo scriptInfo(project.path + "/" + project.mainScript);
    QString dcbName = scriptInfo.baseName() + ".dcb";
    QString sourceDcbPath = scriptInfo.absolutePath() + "/" + dcbName;
    
    if (!QFile::exists(sourceDcbPath)) {
        emit finished(false, "No se encontró el archivo compilado (.dcb).\n"
                             "Por favor, compila el proyecto en el editor antes de publicar.\n"
                             "Esperado en: " + sourceDcbPath);
        return false;
    }
    
    QString destDcbPath = distDir + "/" + baseName + ".dcb";
    QFile::remove(destDcbPath);
    if (!QFile::copy(sourceDcbPath, destDcbPath)) {
        emit finished(false, "Error al copiar el archivo compilado (.dcb).");
        return false;
    }
    
    qDebug() << "Copied DCB from" << sourceDcbPath << "to" << destDcbPath;
    
    // 2. Copy Binaries
    emit progress(40, "Copiando binarios y librerías...");
    
    // Find bgdi in runtime directory first, then PATH
    QString bgdiPath;
    
    if (!runtimeDir.isEmpty()) {
        QString candidate = runtimeDir + "/bgdi";
        if (QFile::exists(candidate)) {
            bgdiPath = candidate;
            qDebug() << "Using bundled bgdi from runtime:" << bgdiPath;
        }
    }
    
    if (bgdiPath.isEmpty()) {
        // Try system PATH
        bgdiPath = QStandardPaths::findExecutable("bgdi");
        if (bgdiPath.isEmpty()) {
            bgdiPath = appDir + "/bgdi";
            if (!QFile::exists(bgdiPath)) {
                emit finished(false, "No se encontró el intérprete bgdi.\n"
                                     "Se buscó en 'runtime/linux-gnu/bgdi' y en el PATH.");
                return false;
            }
        }
    }
    
    QFile::copy(bgdiPath, distDir + "/bgdi");
    QFile(distDir + "/bgdi").setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
    
    // Copy runtime libs (.so) from runtime directory or system
    if (!runtimeDir.isEmpty() && QDir(runtimeDir).exists()) {
        QDir runtimeLibDir(runtimeDir);
        qDebug() << "Copying libraries from runtime directory:" << runtimeDir;
        QStringList filters;
        filters << "*.so*";
        QFileInfoList runtimeLibs = runtimeLibDir.entryInfoList(filters, QDir::Files);
        
        for (const QFileInfo &lib : runtimeLibs) {
            QString destPath = libDir.filePath(lib.fileName());
            QFile::remove(destPath); // Remove if exists
            if (QFile::copy(lib.absoluteFilePath(), destPath)) {
                qDebug() << "Copied runtime lib:" << lib.fileName();
            }
        }
    } else {
        qDebug() << "Runtime directory not found, trying application directory";
        // Fallback: copy from application directory
        QDir binDir(QCoreApplication::applicationDirPath());
        QStringList filters;
        filters << "*.so*";
        QFileInfoList libs = binDir.entryInfoList(filters, QDir::Files);
        
        for (const QFileInfo &lib : libs) {
            QFile::copy(lib.absoluteFilePath(), libDir.filePath(lib.fileName()));
        }
    }
    
    // 3. Copy Assets
    emit progress(60, "Copiando assets...");
    copyDir(project.path + "/assets", assetsDir.absolutePath());
    
    // 4. Create Launcher Script
    // 4. Create Launcher (Wrapper ELF)
    emit progress(80, "Creando lanzador...");
    
    QString wrapperName = "launcher_wrapper_linux";
    QString wrapperSrc = QCoreApplication::applicationDirPath() + "/" + wrapperName;
    if (!QFile::exists(wrapperSrc)) wrapperSrc = QDir::currentPath() + "/" + wrapperName;
    
    if (QFile::exists(wrapperSrc)) {
        QString destWrapper = distDir + "/" + baseName;
        QFile::remove(destWrapper); // Ensure clean state
        QFile::copy(wrapperSrc, destWrapper);
        QFile(destWrapper).setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther | QFile::WriteUser);
        qDebug() << "Launcher wrapper copied to" << destWrapper;
    } else {
        // Fallback to script
        qWarning() << "Launcher wrapper not found, falling back to script.";
        QString scriptPath = distDir + "/run.sh";
        QFile script(scriptPath);
        if (script.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&script);
            out << "#!/bin/sh\n";
            out << "APPDIR=$(dirname \"$(readlink -f \"$0\")\")\n";
            out << "export LD_LIBRARY_PATH=\"$APPDIR/libs:$LD_LIBRARY_PATH\"\n";
            out << "export BENNU_LIB_PATH=\"$APPDIR/libs\"\n"; // Just in case
            out << "cd \"$APPDIR\"\n";
            out << "./bgdi " << baseName << ".dcb\n"; 
            script.close();
            script.setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
        }
    }
    
    // 5. Standalone Executable (Linux ELF)
    if (config.generateLinuxStandalone) {
        emit progress(90, "Creando ejecutable autónomo (Linux)...");
        
        QString stubName = "loader_stub_linux";
        // Search for stub in app dir or current dir
        QString stubPath = QCoreApplication::applicationDirPath() + "/" + stubName;
        if (!QFile::exists(stubPath)) {
            stubPath = QDir::currentPath() + "/" + stubName;
        }

        if (QFile::exists(stubPath)) {
            QString standalonePath = config.outputPath + "/" + baseName + ".AppImage"; // Using .AppImage extension often implies single-file to users, or just .bin
            standalonePath = config.outputPath + "/" + baseName + "_linux.bin"; // Better name
            
            struct FileToEmbed {
                QString sourcePath;
                QString relativePath;
                QByteArray data;
            };
            QVector<FileToEmbed> filesToEmbed;

            // 1. Add BGDI (From distDir where we already copied and renamed it)
            // Or from runtimeDir to be safe. Let's use runtimeDir/bgdi 
            QString bgdiSrc = bgdiPath; // Found earlier
            QFile f(bgdiSrc);
            if (f.open(QIODevice::ReadOnly)) {
                filesToEmbed.append({bgdiSrc, "bgdi", f.readAll()});
                f.close();
            }

            // 2. Add DCB
            QFile fDcb(destDcbPath);
            if (fDcb.open(QIODevice::ReadOnly)) {
                filesToEmbed.append({destDcbPath, baseName + ".dcb", fDcb.readAll()});
                fDcb.close();
            }

            // 3. Add Libraries (.so)
            QDir libDirObj(libDir); // "distDir/libs"
            QFileInfoList libFiles = libDirObj.entryInfoList(QDir::Files);
            for(const auto& info : libFiles) {
                 QFile fLib(info.absoluteFilePath());
                 if (fLib.open(QIODevice::ReadOnly)) {
                     // We put libs in root or lib/ subfolder. Stub sets LD_LIBRARY_PATH to . and ./lib
                     // Let's keep them in root for simplicity or lib/
                     filesToEmbed.append({info.absoluteFilePath(), "lib/" + info.fileName(), fLib.readAll()});
                     fLib.close();
                 }
            }

            // 4. Add Assets (Recursive)
            QString projectDir = QFileInfo(project.path).isDir() ? project.path : QFileInfo(project.path).absolutePath();
            QDirIterator it(projectDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            
            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo info(filePath);
                QString relPath = QDir(projectDir).relativeFilePath(filePath);
                
                if (relPath.startsWith("build") || relPath.startsWith("dist") || relPath.startsWith(".")) continue;
                if (info.suffix() == "dcb" || info.suffix() == "prg") continue;
                if (info.fileName() == "bgdi" || info.fileName().startsWith("loader_stub")) continue;
                
                QFile fAsset(filePath);
                if (fAsset.open(QIODevice::ReadOnly)) {
                    filesToEmbed.append({filePath, relPath, fAsset.readAll()});
                    fAsset.close();
                }
            }
            
            // 5. Add Icon & Desktop (Optional but good)
            if (!project.iconPath.isEmpty() && QFile::exists(project.iconPath)) {
                QFile fIcon(project.iconPath);
                if (fIcon.open(QIODevice::ReadOnly)) {
                     filesToEmbed.append({project.iconPath, "icon.png", fIcon.readAll()});
                     fIcon.close();
                }
                
                // .desktop file
                QString desktopContent = QString("[Desktop Entry]\nType=Application\nName=%1\nExec=AppRun\nIcon=icon\n")
                                         .arg(project.name);
                filesToEmbed.append({"", baseName + ".desktop", desktopContent.toUtf8()});
            }

            // WRITE
            QFile stubFile(stubPath);
            QFile outFile(standalonePath);
            
            if (stubFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
                outFile.write(stubFile.readAll());
                stubFile.close();
                
                // TOC
                 struct TOCEntry {
                    char path[256];
                    uint32_t size;
                };
                QVector<TOCEntry> toc;
                
                for(const auto& file : filesToEmbed) {
                    outFile.write(file.data); // Write Data
                    
                    TOCEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    QByteArray pathBytes = file.relativePath.toUtf8();
                    // Normal slashes for Linux
                    strncpy(entry.path, pathBytes.constData(), 255);
                    entry.size = (uint32_t)file.data.size();
                    toc.append(entry);
                }
                
                // Write TOC
                 for(const auto& entry : toc) {
                    outFile.write((const char*)&entry, sizeof(entry));
                }
                
                // Write Footer
                struct PayloadFooterV3 {
                    char magic[32]; // BENNUGD2_PAYLOAD_V3
                    uint32_t numFiles;
                } footer;
                 memset(&footer, 0, sizeof(footer));
                strncpy(footer.magic, "BENNUGD2_PAYLOAD_V3", 31);
                footer.numFiles = (uint32_t)toc.size();
                
                outFile.write((const char*)&footer, sizeof(footer));
                outFile.close();
                
                // Make executable
                outFile.setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
                
                qDebug() << "Created Linux Standalone:" << standalonePath;
            } else {
                 qWarning() << "Failed to create Linux Standalone";
            }

        } else {
            qWarning() << "Linux loader stub not found:" << stubPath;
             // Continue without failing entire publish
        }
    }

    // 6. Compress / AppImage
    if (config.generateAppImage) {
        emit progress(90, "Generando AppImage...");
        
        // AppDir Structure
        QString appDir = config.outputPath + "/AppDir";
        
        // Clean previous AppDir to avoid stale files
        QDir cleanAppDir(appDir);
        if (cleanAppDir.exists()) cleanAppDir.removeRecursively();
        
        QDir().mkpath(appDir + "/usr/bin");
        QDir().mkpath(appDir + "/usr/lib");
        QDir().mkpath(appDir + "/usr/share/icons/hicolor/256x256/apps");
        
        // Move assets to safe place
        // Actually, AppRun needs to set paths.
        // Let's create a standard AppDir
        
        // 1. Copy Binary
        QFile::copy(distDir + "/" + baseName, appDir + "/usr/bin/" + baseName);
        QFile(appDir + "/usr/bin/" + baseName).setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
        
        // Copy DCB to AppDir/usr/bin
        QFile::copy(distDir + "/" + baseName + ".dcb", appDir + "/usr/bin/" + baseName + ".dcb");

        // 2. Copy Libs
        copyDir(libDir.absolutePath(), appDir + "/usr/lib");
        
        // 3. Copy Assets relative to binary (classic method) or in share (standard)
        // Bennu usually expects assets near binary or in current dir. AppRun handles current dir.
        // We will put assets in usr/bin so they are next to executable, easiest for Bennu.
        copyDir(assetsDir.absolutePath(), appDir + "/usr/bin/assets");
        
        // Remove tmp dist dir if we are only making appimage? No, keep it as "unpacked" version maybe?
        // User asked for .tar.gz AND AppImage possibly.
        
        // 4. Create AppRun
        QFile appRun(appDir + "/AppRun");
        if (appRun.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&appRun);
            out << "#!/bin/sh\n";
            out << "HERE=\"$(dirname \"$(readlink -f \"${0}\")\")\"\n";
            out << "export LD_LIBRARY_PATH=\"${HERE}/usr/lib:$LD_LIBRARY_PATH\"\n";
            out << "export BENNU_LIB_PATH=\"${HERE}/usr/lib\"\n"; 
            out << "cd \"${HERE}/usr/bin\"\n";
            out << "./" << baseName << " " << baseName << ".dcb\n"; // Run bgdi dcb
            appRun.close();
            appRun.setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
        }
        
        // 5. Desktop File
        QFile desktop(appDir + "/" + baseName + ".desktop");
        if (desktop.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&desktop);
            out << "[Desktop Entry]\n";
            out << "Type=Application\n";
            out << "Name=" << project.name << "\n";
            out << "Exec=" << baseName << "\n";
            out << "Icon=" << baseName << "\n";
            out << "Categories=Game;\n";
            out << "Terminal=false\n";
            desktop.close();
        }
        
        // 6. Icon
        QString iconDest = appDir + "/" + baseName + ".png";
        QString dirIconDest = appDir + "/.DirIcon";
        
        if (!config.iconPath.isEmpty() && QFile::exists(config.iconPath)) {
             QFile::copy(config.iconPath, iconDest);
             QFile::copy(config.iconPath, dirIconDest);
        } else {
             // Fallback: Create a simple colored pixmap as icon
             QImage dummyIcon(256, 256, QImage::Format_ARGB32);
             dummyIcon.fill(QColor(42, 130, 218)); // Bennuish Blue
             QPainter p(&dummyIcon);
             p.setPen(Qt::white);
             QFont f = p.font();
             f.setPixelSize(100);
             f.setBold(true);
             p.setFont(f);
             p.drawText(dummyIcon.rect(), Qt::AlignCenter, "B2");
             p.end();
             
             dummyIcon.save(iconDest);
             dummyIcon.save(dirIconDest);
        }
        
        // 7. Run appimagetool
        QProcess appImageTool;
        appImageTool.setWorkingDirectory(config.outputPath);
        
        QString toolExe = "appimagetool";
        if (!config.appImageToolPath.isEmpty() && QFile::exists(config.appImageToolPath)) {
            toolExe = config.appImageToolPath;
            // Ensure executable
            QFile::setPermissions(toolExe, QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner);
        } else {
             // Check if system tool exists
             if (QStandardPaths::findExecutable("appimagetool").isEmpty()) {
                 emit finished(true, "AppDir creado en " + appDir + ".\nInstala 'appimagetool' o configúralo para generar el archivo final.");
                 return true; 
             }
        }
        
        // Prepare environment
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("ARCH", "x86_64"); // Force architecture detection
        appImageTool.setProcessEnvironment(env);
        
        // Ensure AppRun is executable
        QFile(appDir + "/AppRun").setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
        
        // Check file size (sanity check)
        QFileInfo toolInfo(toolExe);
        if (toolInfo.size() < 1024 * 1024) { // Less than 1MB is suspicious
             emit finished(false, "Error: El archivo appimagetool parece corrupto o incompleto (" + QString::number(toolInfo.size()) + " bytes).\n"
                                  "Por favor borra " + toolExe + " y vuelve a descargarlo desde el diálogo.");
             return false;
        }

        // Diagnosis: Check if appimagetool runs at all
        QProcess checkRun;
        checkRun.setProcessEnvironment(env);
        checkRun.start(toolExe, QStringList() << "--version");
        if (!checkRun.waitForFinished() || checkRun.exitCode() != 0) {
             // Try running with APPIMAGE_EXTRACT_AND_RUN=1 (No FUSE needed)
             qDebug() << "Standard execution failed. Trying APPIMAGE_EXTRACT_AND_RUN=1...";
             
             env.insert("APPIMAGE_EXTRACT_AND_RUN", "1");
             appImageTool.setProcessEnvironment(env);
             
             // Check again
             QProcess checkRun2;
             checkRun2.setProcessEnvironment(env);
             checkRun2.start(toolExe, QStringList() << "--version");
             
             if (!checkRun2.waitForFinished() || checkRun2.exitCode() != 0) {
                 QString err = checkRun2.readAllStandardError(); // Read from checkRun2
                 QString out = checkRun2.readAllStandardOutput();
                 emit finished(false, "No se puede ejecutar appimagetool incluso sin FUSE.\n"
                                      "Código de salida: " + QString::number(checkRun2.exitCode()) + "\n"
                                      "Salida (stdout): " + out + "\n"
                                      "Error (stderr): " + err + "\n\n"
                                      "Posibles soluciones:\n"
                                      "1. Instala libfuse2: sudo apt install libfuse2\n"
                                      "2. Borra el archivo y redescárgalo.");
                 return false;
             }
        }

        // Run with --no-appstream to avoid validation errors and --verbose for debug
        // Note: appImageTool environment might have been updated with EXTRACT_AND_RUN above
        
        QString finalAppImagePath = config.outputPath + "/" + baseName + ".AppImage";
        if (QFile::exists(finalAppImagePath)) QFile::remove(finalAppImagePath);
        
        appImageTool.start(toolExe, QStringList() << "--no-appstream" << "--verbose" << "AppDir" << baseName + ".AppImage");
        if (appImageTool.waitForFinished() && appImageTool.exitCode() == 0) {
            // Cleanup AppDir if successful? optional.
        } else {
             QString error = appImageTool.readAllStandardError();
             QString output = appImageTool.readAllStandardOutput();
             QString msg = "Error ejecutando appimagetool (" + toolExe + "):\n";
             if (!error.isEmpty()) msg += error;
             else if (!output.isEmpty()) msg += output;
             else msg += "Código de salida: " + QString::number(appImageTool.exitCode());
             
             emit finished(true, msg); // Emit true so dialog stays open but warns? Or false? 
             // If we return 'true' here, it says "Publication Successful" then shows error msg.
             // Better emit 'false' but since the tar.gz MIGHT have succeeded...
             // Let's emit finished with false to show the error clearly.
             // Actually, if we are here, we might have already done tar.gz? 
             // No, tar.gz is after this block in my code logic!
             
             // So if AppImage fails, we fail completely (or we should continue to tar.gz?)
             // Let's fail for now to show the error.
             emit finished(false, msg);
             return false; 
        }
    } 
    
    // Always do tar.gz if requested (default logic was else, changed to sequential if checkboxes allow)
    // Actually the dialog has checkboxes. Publisher::publishLinux only receives `config`.
    // We should check config.
    
    // Assuming config allows both, but publisher.cpp was structured as if/else.
    // The previous code had "if (config.generateAppImage) {} else { tar }".
    // I should probably support both.
    
    // Let's just create tar.gz as primary artifact always for now unless I rewrite structure.
    // I'll leave the tar logic as is (it runs if AppImage is FALSE in my previous code, wait).
    // My previous code: "if (config.generateAppImage) { ... } else { emit progress... tar ... }"
    // That prevents both.
    
    // Correct logic:
    // We already created the dir structure for tar.gz in steps 1-4.
    // So assume that folder IS the tar.gz source.
    
    // AppImage logic above creates a SEPARATE AppDir.
    
    if (config.generateLinuxArchive) {
        emit progress(95, "Comprimiendo (.tar.gz)...");
        QProcess tar;
        tar.setWorkingDirectory(config.outputPath);
        QStringList tarArgs;
        tarArgs << "-czf" << baseName + ".tar.gz" << baseName;
        tar.start("tar", tarArgs);
        tar.waitForFinished();
    }
    
    emit progress(100, "¡Listo!");
    return true;
}

bool Publisher::publishAndroid(const ProjectData &project, const PublishConfig &config)
{
    // ... (existing beginning)
    emit progress(10, "Preparando proyecto Android...");
    
    // Generate internal structure (No external template dependency)
    QString targetName = config.packageName.section('.', -1); 
    QString targetDir = config.outputPath + "/" + targetName;
    
    // 1. Root structure
    QDir().mkpath(targetDir + "/app/src/main/assets");
    QDir().mkpath(targetDir + "/app/src/main/java");
    QDir().mkpath(targetDir + "/app/src/main/res/values");
    QDir().mkpath(targetDir + "/gradle/wrapper");
    
    // 2. gradle.properties
    QFile gp(targetDir + "/gradle.properties");
    if (gp.open(QIODevice::WriteOnly)) {
        QTextStream(&gp) << "org.gradle.jvmargs=-Xmx2048m -Dfile.encoding=UTF-8\n"
                         << "android.useAndroidX=true\n"
                         << "android.enableJetifier=true\n";
        gp.close();
    }
    
    // 3. settings.gradle
    QFile sg(targetDir + "/settings.gradle");
    if (sg.open(QIODevice::WriteOnly)) {
        QTextStream(&sg) << "include ':app'\nrootProject.name = \"" << targetName << "\"\n";
        sg.close();
    }
    
    // 4. local.properties (NDK location)
    QString ndkHome = qgetenv("ANDROID_NDK_HOME");
    if (!ndkHome.isEmpty()) {
        QFile lp(targetDir + "/local.properties");
        if (lp.open(QIODevice::WriteOnly)) {
             QTextStream(&lp) << "ndk.dir=" << ndkHome << "\n"
                              << "sdk.dir=" << QStandardPaths::writableLocation(QStandardPaths::HomeLocation) << "/Android/Sdk\n"; // Fallback SDK
             lp.close();
        }
    }
    
    // 5. build.gradle (Root)
    QFile bgr(targetDir + "/build.gradle");
    if (bgr.open(QIODevice::WriteOnly)) {
        QTextStream(&bgr) << "buildscript {\n"
                          << "    repositories {\n"
                          << "        google()\n"
                          << "        mavenCentral()\n"
                          << "    }\n"
                          << "    dependencies {\n"
                          << "        classpath 'com.android.tools.build:gradle:8.1.1'\n"
                          << "    }\n"
                          << "}\n"
                          << "allprojects {\n"
                          << "    repositories {\n"
                          << "        google()\n"
                          << "        mavenCentral()\n"
                          << "    }\n"
                          << "}\n";
        bgr.close();
    }
    
    // 6. app/build.gradle
    QFile bga(targetDir + "/app/build.gradle");
    if (bga.open(QIODevice::WriteOnly)) {
        QTextStream(&bga) << "plugins {\n"
                          << "    id 'com.android.application'\n"
                          << "}\n\n"
                          << "android {\n"
                          << "    namespace '" << config.packageName << "'\n"
                          << "    compileSdk 34\n\n"
                          << "    defaultConfig {\n"
                          << "        applicationId '" << config.packageName << "'\n"
                          << "        minSdk 21\n"
                          << "        targetSdk 34\n"
                          << "        versionCode 1\n"
                          << "        versionName \"1.0\"\n"
                          << "        ndk {\n"
                          << "            abiFilters 'armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'\n"
                          << "        }\n"
                          << "    }\n\n"
                          << "    buildTypes {\n"
                          << "        release {\n"
                          << "            minifyEnabled false\n"
                          << "            proguardFiles getDefaultProguardFile('proguard-android-optimize.txt'), 'proguard-rules.pro'\n"
                          << "        }\n"
                          << "    }\n"
                          << "}\n\n"
                          << "dependencies {\n"
                          << "    implementation 'androidx.appcompat:appcompat:1.6.1'\n"
                          << "}\n";
        bga.close();
    }
    
    // 7. gradlew wrapper (We should download current wrapper or write a basic script)
    // To be truly professional, we should carry the gradle-wrapper.jar and properties.
    // For now, let's write a simple shell script asking user to init gradle or just assume they have 'gradle' in path if wrapper fails.
    // BETTER: Download wrapper if missing.
    // Actually, writing a minimal gradlew script that invokes 'gradle' is safer if we don't bundle the jar.
    
    // 8. strings.xml
    QFile strings(targetDir + "/app/src/main/res/values/strings.xml");
    if (strings.open(QIODevice::WriteOnly)) {
        QTextStream(&strings) << "<resources>\n    <string name=\"app_name\">" << project.name << "</string>\n</resources>\n";
        strings.close();
    }
    
    // Continue with java generation...
    
    QString javaSrc = targetDir + "/app/src/main/java";
    
    QString packagePath = config.packageName;
    packagePath.replace(".", "/");
    QString newJavaPath = javaSrc + "/" + packagePath;
    QDir().mkpath(newJavaPath);
    
    // Activity Generation (Same as before)
    QString activityName = "Activity_" + project.name.simplified().replace(" ", "_");
    QString activityFile = newJavaPath + "/" + activityName + ".java";
    
    QFile java(activityFile);
    if (java.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&java);
        out << "package " << config.packageName << ";\n\n";
        out << "import org.libsdl.app.SDLActivity;\n";
        out << "import org.libsdl.app.AdsModule;\n";
        out << "import org.libsdl.app.IAPModule;\n";
        out << "import android.os.Bundle;\n\n";
        out << "public class " << activityName << " extends SDLActivity {\n";
        out << "    @Override\n";
        out << "    protected void onCreate(Bundle savedInstanceState) {\n";
        out << "        super.onCreate(savedInstanceState);\n";
        out << "        AdsModule.initialize(this);\n";
        out << "        IAPModule.initialize(this);\n";
        out << "    }\n";
        out << "    @Override\n";
        out << "    protected void onPause() {\n";
        out << "        super.onPause();\n";
        out << "        AdsModule.hideBanner();\n";
        out << "    }\n";
        out << "}\n";
        java.close();
    }
    
    // We also need Copies AdsModule and IAPModule if they are not in template.
    // They are usually in modules/libmod_ads/...
    // Let's try to find them in source if possible.
    // 4. Copy pre-compiled libraries (libs)
    emit progress(60, "Copiando librerías...");
    
    QDir appDir(QCoreApplication::applicationDirPath());
    appDir.cdUp(); // linux-gnu
    appDir.cdUp(); // build
    appDir.cdUp(); // BennuGD2 root
    
    QString modulesDir = appDir.absoluteFilePath("modules"); // Now works
    QString adsJava = modulesDir + "/libmod_ads/AdsModule.java";
    QString iapJava = modulesDir + "/libmod_iap/IAPModule.java";
    
    QString sdlPackagePath = javaSrc + "/org/libsdl/app";
    QDir().mkpath(sdlPackagePath);
    
    if (QFile::exists(adsJava)) QFile::copy(adsJava, sdlPackagePath + "/AdsModule.java");
    if (QFile::exists(iapJava)) QFile::copy(iapJava, sdlPackagePath + "/IAPModule.java");
    
    // Copy Android native libraries (matching create_android_project.sh logic)
    // We need to copy from 3 sources:
    // 1. build/toolchain/bin/*.so (BennuGD runtime and modules)
    // 2. vendor/android/toolchain/abi/lib/*.so (SDL2, ogg, vorbis, theora)
    // 3. vendor/sdl-gpu/build/toolchain/SDL_gpu/lib/libSDL2_gpu.so
    
    emit progress(65, "Copiando librerías nativas...");
    
    // Find project root by searching up from application dir
    QDir searchDir(QCoreApplication::applicationDirPath());
    QString projectRoot;
    for (int i=0; i<8; i++) {
        if (searchDir.exists("vendor") && searchDir.exists("build")) {
            projectRoot = searchDir.absolutePath();
            break;
        }
        if (searchDir.isRoot() || !searchDir.cdUp()) break;
    }
    
    if (projectRoot.isEmpty()) {
        qDebug() << "Error: Could not find BennuGD2 project root";
        emit progress(65, "ERROR: No se encontró el directorio raíz de BennuGD2");
    } else {
        // Map toolchain names to Android ABI names
        QMap<QString, QString> toolchainToAbi;
        toolchainToAbi["armv7a-linux-androideabi"] = "armeabi-v7a";
        toolchainToAbi["aarch64-linux-android"] = "arm64-v8a";
        toolchainToAbi["i686-linux-android"] = "x86";
        toolchainToAbi["x86_64-linux-android"] = "x86_64";
        
        QString jniLibsDir = targetDir + "/app/src/main/jniLibs";
        
        bool hasBennuLibs = false;
        bool hasVendorLibs = false;
        
        for (auto it = toolchainToAbi.constBegin(); it != toolchainToAbi.constEnd(); ++it) {
            QString toolchain = it.key();
            QString abi = it.value();
            
            QString abiLibDir = jniLibsDir + "/" + abi;
            QDir().mkpath(abiLibDir);
            
            int copiedCount = 0;
            
            // 1. Copy BennuGD libraries from build/toolchain/bin/
            QString buildBinDir = projectRoot + "/build/" + toolchain + "/bin";
            if (QDir(buildBinDir).exists()) {
                QDir binDir(buildBinDir);
                QStringList filters; 
                filters << "*.so";
                
                for (const QFileInfo &soFile : binDir.entryInfoList(filters, QDir::Files)) {
                    QString destPath = abiLibDir + "/" + soFile.fileName();
                    QFile::remove(destPath);
                    if (QFile::copy(soFile.absoluteFilePath(), destPath)) {
                        copiedCount++;
                        hasBennuLibs = true;
                    }
                }
            }
            
            // 2. Copy SDL2/vendor libraries from vendor/android/toolchain/abi/lib/
            QString vendorLibDir = projectRoot + "/vendor/android/" + toolchain + "/" + abi + "/lib";
            if (QDir(vendorLibDir).exists()) {
                QDir libDir(vendorLibDir);
                QStringList filters; 
                filters << "*.so" << "*.so.*";
                
                for (const QFileInfo &soFile : libDir.entryInfoList(filters, QDir::Files)) {
                    QString destPath = abiLibDir + "/" + soFile.fileName();
                    QFile::remove(destPath);
                    if (QFile::copy(soFile.absoluteFilePath(), destPath)) {
                        copiedCount++;
                        hasVendorLibs = true;
                    }
                }
            }
            
            // 3. Copy SDL_gpu from vendor/sdl-gpu/build/toolchain/SDL_gpu/lib/
            QString sdlGpuLib = projectRoot + "/vendor/sdl-gpu/build/" + toolchain + "/SDL_gpu/lib/libSDL2_gpu.so";
            if (QFile::exists(sdlGpuLib)) {
                QString destPath = abiLibDir + "/libSDL2_gpu.so";
                QFile::remove(destPath);
                if (QFile::copy(sdlGpuLib, destPath)) {
                    copiedCount++;
                }
            }
            
            qDebug() << "Copied" << copiedCount << "libraries to" << abi;
        }
        
        // Warn if BennuGD libraries are missing
        if (!hasBennuLibs) {
            qDebug() << "WARNING: No BennuGD libraries found in build/. Compile BennuGD for Android first.";
            emit progress(70, "ADVERTENCIA: Faltan librerías de BennuGD. Compila BennuGD para Android primero.");
        }
        
        if (!hasVendorLibs) {
            qDebug() << "WARNING: No vendor libraries found. Run build-android-deps.sh first.";
        }
    }
    
    // Manifest & Gradle (Same as before)
    emit progress(40, "Configurando Manifiesto...");
    QString manifestPath = targetDir + "/app/src/main/AndroidManifest.xml";
    QFile manifest(manifestPath);
    if (manifest.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = manifest.readAll();
        manifest.close();
        content.replace("package=\"org.libsdl.app\"", "package=\"" + config.packageName + "\"");
        content.replace("android:name=\"SDLActivity\"", "android:name=\"." + activityName + "\"");
        if (manifest.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&manifest);
            out << content;
            manifest.close();
        }
    }
    
    QString gradlePath = targetDir + "/app/build.gradle";
    QFile gradle(gradlePath);
    if (gradle.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = gradle.readAll();
        gradle.close();
        content.replace("org.libsdl.app", config.packageName);
        if (gradle.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&gradle);
            out << content;
            gradle.close();
        }
    }
    
    // 3. Copy Compile Game (.dcb)
    emit progress(60, "Copiando binario compilado...");
    
    // Determine source DCB path based on mainScript
    QFileInfo scriptInfo(project.path + "/" + project.mainScript);
    QString dcbName = scriptInfo.baseName() + ".dcb";
    QString sourceDcbPath = scriptInfo.absolutePath() + "/" + dcbName;
    
    if (!QFile::exists(sourceDcbPath)) {
        emit finished(false, "No se encontró el archivo compilado (.dcb).\n"
                             "Por favor, compila el proyecto en el editor antes de generar para Android.");
        return false;
    }
    
    QString dcbPath = targetDir + "/app/src/main/assets/main.dcb";
    QDir().mkpath(targetDir + "/app/src/main/assets");
    
    QFile::remove(dcbPath);
    if (!QFile::copy(sourceDcbPath, dcbPath)) {
        emit finished(false, "Error al copiar el archivo compilado (.dcb).");
        return false;
    }
    
    // 4. Copy Assets (Same)
    emit progress(70, "Copiando assets...");
    copyDir(project.path + "/assets", targetDir + "/app/src/main/assets");
    
    // 5. Copy Libs (Handled earlier via simple vendor copy)
    // Legacy/Complex logic removed to favor direct 'vendor' copy.
    
    // 6. Build APK
    if (config.generateAPK) {
        emit progress(80, "Intentando generar APK...");
        QProcess gradleProc;
        gradleProc.setWorkingDirectory(targetDir);
        
        // Make gradlew executable
        QFile(targetDir + "/gradlew").setPermissions(QFile::ExeUser | QFile::ExeOwner | QFile::ReadOwner | QFile::WriteOwner);
        
        gradleProc.start("./gradlew", QStringList() << "assembleDebug");
        if (gradleProc.waitForFinished() && gradleProc.exitCode() == 0) {
             emit progress(100, "APK Generado!");
             return true;
        } else {
             emit finished(false, "Falló la compilación de Gradle. Posiblemente faltan librerías .so de BennuGD en 'jniLibs'.");
             return false;
        }
    }
    
    emit progress(100, "Proyecto Android Generado. Verifica carpeta jniLibs.");
    return true; // Return true even if apk skipped
}

bool Publisher::publishWindows(const ProjectData &project, const PublishConfig &config)
{
    emit progress(10, "Preparando entorno Windows...");
    
    QString baseName = project.name.simplified().replace(" ", "_");
    QString distDir = config.outputPath + "/" + baseName + "_win64";
    
    // Clean previous output
    QDir dir(distDir);
    if (dir.exists()) dir.removeRecursively();
    dir.mkpath(".");
    
    QDir assetsDir(distDir + "/assets");
    assetsDir.mkpath(".");
    
    // Find runtime directory (for bgdi.exe and DLLs)
    QString appDir = QCoreApplication::applicationDirPath();
    QDir searchDir(appDir);
    QString runtimeDir;
    
    for (int i = 0; i < 4; i++) {
        QString candidate = searchDir.absoluteFilePath("runtime/win64");
        if (QDir(candidate).exists()) {
            runtimeDir = candidate;
            qDebug() << "Found Windows runtime dir:" << runtimeDir;
            break;
        }
        searchDir.cdUp();
    }
    
    if (runtimeDir.isEmpty()) {
        emit finished(false, "No se encontró el directorio runtime/win64.\n"
                             "Asegúrate de tener los binarios de Windows en runtime/win64/");
        return false;
    }
    
    // 1. Copy Compiled Game (.dcb)
    emit progress(20, "Copiando binario compilado...");
    
    QFileInfo scriptInfo(project.path + "/" + project.mainScript);
    QString dcbName = scriptInfo.baseName() + ".dcb";
    QString sourceDcbPath = scriptInfo.absolutePath() + "/" + dcbName;
    
    if (!QFile::exists(sourceDcbPath)) {
        emit finished(false, "No se encontró el archivo compilado (.dcb).\n"
                             "Por favor, compila el proyecto en el editor antes de publicar.\n"
                             "Esperado en: " + sourceDcbPath);
        return false;
    }
    
    QString destDcbPath = distDir + "/" + baseName + ".dcb";
    QFile::remove(destDcbPath);
    if (!QFile::copy(sourceDcbPath, destDcbPath)) {
        emit finished(false, "Error al copiar el archivo compilado (.dcb).");
        return false;
    }
    
    qDebug() << "Copied DCB from" << sourceDcbPath << "to" << destDcbPath;
    
    // 2. Copy bgdi.exe and create launcher
    emit progress(40, "Copiando ejecutable de Windows...");
    
    QString bgdiExePath = runtimeDir + "/bgdi.exe";
    if (!QFile::exists(bgdiExePath)) {
        emit finished(false, "No se encontró bgdi.exe en " + runtimeDir + "\n"
                             "Asegúrate de tener el ejecutable de Windows en runtime/win64/bgdi.exe");
        return false;
    }
    
    // Copy bgdi.exe to the dist directory
    QString destBgdiPath = distDir + "/bgdi.exe";
    QFile::remove(destBgdiPath);
    if (!QFile::copy(bgdiExePath, destBgdiPath)) {
        emit finished(false, "Error al copiar bgdi.exe");
        return false;
    }
    
    qDebug() << "Copied bgdi.exe to" << destBgdiPath;
    
    // Create a launcher .bat file
    QString launcherBatPath = distDir + "/" + baseName + ".bat";
    QFile launcherBat(launcherBatPath);
    if (launcherBat.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&launcherBat);
        out << "@echo off\r\n";
        out << "cd /d \"%~dp0\"\r\n";
        out << "bgdi.exe \"" << baseName << ".dcb\"\r\n";
        launcherBat.close();
        qDebug() << "Created launcher .bat:" << launcherBatPath;
    } else {
        qWarning() << "Failed to create launcher .bat";
    }
    
    // Also create a VBS launcher that runs without console window
    QString launcherVbsPath = distDir + "/" + baseName + ".vbs";
    QFile launcherVbs(launcherVbsPath);
    if (launcherVbs.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&launcherVbs);
        out << "Set WshShell = CreateObject(\"WScript.Shell\")\r\n";
        out << "WshShell.CurrentDirectory = CreateObject(\"Scripting.FileSystemObject\").GetParentFolderName(WScript.ScriptFullName)\r\n";
        out << "WshShell.Run \"bgdi.exe \"\"" << baseName << ".dcb\"\"\", 0, False\r\n";
        launcherVbs.close();
        qDebug() << "Created launcher .vbs:" << launcherVbsPath;
    } else {
        qWarning() << "Failed to create launcher .vbs";
    }
    
    // Create a README with instructions
    QString readmePath = distDir + "/README.txt";
    QFile readme(readmePath);
    if (readme.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&readme);
        out << project.name << " - Windows Edition\r\n";
        out << "========================================\r\n\r\n";
        out << "Para ejecutar el juego:\r\n";
        out << "1. Haz doble clic en '" << baseName << ".bat'\r\n";
        out << "   (o '" << baseName << ".vbs' para ejecutar sin ventana de consola)\r\n\r\n";
        out << "Archivos incluidos:\r\n";
        out << "- bgdi.exe: Motor de ejecución de BennuGD\r\n";
        out << "- " << baseName << ".dcb: Código compilado del juego\r\n";
        out << "- *.dll: Librerías necesarias\r\n";
        out << "- assets/: Recursos del juego\r\n\r\n";
        out << "Versión: " << project.version << "\r\n";
        readme.close();
        qDebug() << "Created README.txt";
    }
    
    // 3. Copy DLLs
    emit progress(60, "Copiando librerías DLL...");
    
    QDir runtimeDirObj(runtimeDir);
    QStringList dllFilters;
    dllFilters << "*.dll";
    QFileInfoList dllFiles = runtimeDirObj.entryInfoList(dllFilters, QDir::Files);
    
    int copiedDlls = 0;
    for (const QFileInfo &dllInfo : dllFiles) {
        QString destDllPath = distDir + "/" + dllInfo.fileName();
        QFile::remove(destDllPath);
        if (QFile::copy(dllInfo.absoluteFilePath(), destDllPath)) {
            copiedDlls++;
            qDebug() << "Copied DLL:" << dllInfo.fileName();
        } else {
            qWarning() << "Failed to copy DLL:" << dllInfo.fileName();
        }
    }
    
    qDebug() << "Copied" << copiedDlls << "DLL files";
    
    // 4. Copy Assets
    emit progress(80, "Copiando assets...");
    
    QString projectAssetsDir = project.path + "/assets";
    if (QDir(projectAssetsDir).exists()) {
        if (!copyDir(projectAssetsDir, distDir + "/assets")) {
            qWarning() << "Error al copiar algunos assets";
        }
    }
    
    // Copy FPG if specified
    if (!project.fpgFile.isEmpty()) {
        QString fpgPath = project.path + "/" + project.fpgFile;
        if (QFile::exists(fpgPath)) {
            QFileInfo fpgInfo(fpgPath);
            QString destFpgPath = distDir + "/assets/" + fpgInfo.fileName();
            QFile::remove(destFpgPath);
            QFile::copy(fpgPath, destFpgPath);
        }
    }
    
    // Copy map files
    QDir projectDir(project.path);
    QStringList mapFilters;
    mapFilters << "*.raymap" << "*.wld" << "*.map";
    QFileInfoList mapFiles = projectDir.entryInfoList(mapFilters, QDir::Files);
    
    for (const QFileInfo &mapInfo : mapFiles) {
        QString destMapPath = distDir + "/assets/" + mapInfo.fileName();
        QFile::remove(destMapPath);
        QFile::copy(mapInfo.absoluteFilePath(), destMapPath);
    }
    
    // 5. Create standalone executable with embedded resources (if requested)
    bool createdStandalone = false;
    QString standaloneExePath = config.outputPath + "/" + baseName + ".exe";
    
    if (config.generateStandalone) {
        emit progress(85, "Creando ejecutable autónomo...");
        
        // Find the pre-compiled loader stub
        QString appDir = QCoreApplication::applicationDirPath();
        QString stubPath = appDir + "/loader_stub.exe";
        
        // If not found in app dir (dev environment), look in current dir
        if (!QFile::exists(stubPath)) {
            stubPath = QDir::currentPath() + "/loader_stub.exe";
        }
        
        if (QFile::exists(stubPath)) {
            qDebug() << "Found loader stub at:" << stubPath;
            
            // --- V3 PACKAGING SYSTEM ---
            // We needed a more flexible way to include assets recursively.
            // Format: [STUB] + [FILE_1_DATA] + ... + [FILE_N_DATA] + [TOC] + [FOOTER]
            
            struct FileToEmbed {
                QString sourcePath;
                QString relativePath; // Path relative to execution root (e.g. "bgdi.exe", "assets/player.png")
                QByteArray data;
            };
            QVector<FileToEmbed> filesToEmbed;
            
            // 1. Add BGDI.EXE
            QFile bgdiFile(destBgdiPath);
            if (bgdiFile.open(QIODevice::ReadOnly)) {
                filesToEmbed.append({destBgdiPath, "bgdi.exe", bgdiFile.readAll()});
                bgdiFile.close();
            } else {
                 emit finished(false, "No se pudo leer bgdi.exe para empaquetado.");
                 return false;
            }
            
            // 2. Add Game DCB
            QFile dcbFile(destDcbPath);
            QString dcbName = baseName + ".dcb";
            if (dcbFile.open(QIODevice::ReadOnly)) {
                filesToEmbed.append({destDcbPath, dcbName, dcbFile.readAll()});
                dcbFile.close();
                qDebug() << "Added main game file:" << dcbName;
            } else {
                 emit finished(false, "No se pudo leer el archivo .dcb para empaquetado.");
                 return false;
            }

            // 3. Add DLLs
            QDir runtimeDirObj(runtimeDir);
            QStringList dllFilters;
            dllFilters << "*.dll";
            QFileInfoList dllFiles = runtimeDirObj.entryInfoList(dllFilters, QDir::Files);
            for (const QFileInfo &dllInfo : dllFiles) {
                QFile f(dllInfo.absoluteFilePath());
                if (f.open(QIODevice::ReadOnly)) {
                    filesToEmbed.append({dllInfo.absoluteFilePath(), dllInfo.fileName(), f.readAll()});
                    f.close();
                }
            }
            
            // 4. Add ASSETS (Recursive from project dir)
            // We want to include everything in the project dir EXCEPT:
            // - The .dcb file (already added)
            // - The build/dist directories
            // - Hidden files
            // - Source code files (.prg, .c, .h, .cpp) - Optional: user might want to hide source
            
            QString projectDir = QFileInfo(project.path).isDir() ? project.path : QFileInfo(project.path).absolutePath();
            QDir projectDirObj(projectDir);
            
            // Use QDirIterator for recursive scanning
            QDirIterator it(projectDir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            
            qDebug() << "Scanning for assets in:" << projectDir;
            int assetsCount = 0;
            
            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo info(filePath);
                QString relPath = projectDirObj.relativeFilePath(filePath);
                
                // Filtering
                if (relPath.startsWith("build") || relPath.startsWith("dist") || relPath.startsWith(".")) continue;
                if (info.suffix() == "dcb" || info.suffix() == "prg") continue; // Skip redundant binaries/source
                if (info.fileName() == "bgdi.exe" || info.fileName() == "loader_stub.exe") continue;
                
                QFile f(filePath);
                if (f.open(QIODevice::ReadOnly)) {
                    filesToEmbed.append({filePath, relPath, f.readAll()});
                    f.close();
                    assetsCount++;
                }
            }
            qDebug() << "Added" << assetsCount << "asset files.";

            // --- WRITE OUTPUT ---
            QFile stubFile(stubPath);
            QFile outFile(standaloneExePath);
            
            if (stubFile.open(QIODevice::ReadOnly) && outFile.open(QIODevice::WriteOnly)) {
                
                // A. Write Stub
                outFile.write(stubFile.readAll());
                stubFile.close();
                
                // B. Write Files Data
                struct TOCEntry {
                    char path[256];
                    uint32_t size;
                };
                QVector<TOCEntry> toc;
                
                for(const auto& file : filesToEmbed) {
                    outFile.write(file.data);
                    
                    TOCEntry entry;
                    memset(&entry, 0, sizeof(entry));
                    
                    // Convert relative path to standard backslashes for Windows, or keep forward slash (stub handles normalization)
                    QByteArray pathBytes = file.relativePath.toUtf8();
                    strncpy(entry.path, pathBytes.constData(), 255);
                    entry.size = (uint32_t)file.data.size();
                    
                    toc.append(entry);
                }
                
                // C. Write TOC
                for(const auto& entry : toc) {
                    outFile.write((const char*)&entry, sizeof(entry));
                }
                
                // D. Write Footer V3
                struct PayloadFooterV3 {
                    char magic[32]; // BENNUGD2_PAYLOAD_V3
                    uint32_t numFiles;
                } footer;
                
                memset(&footer, 0, sizeof(footer));
                strncpy(footer.magic, "BENNUGD2_PAYLOAD_V3", 31);
                footer.numFiles = (uint32_t)toc.size();
                
                outFile.write((const char*)&footer, sizeof(footer));
                outFile.close();
                
                createdStandalone = true;
                qDebug() << "Created V3 standalone executable with" << toc.size() << "total files.";
                
            } else {
                 emit finished(false, "Error al escribir el ejecutable autónomo.");
                 return false;
            }
        } else {
            qWarning() << "Loader stub not found at:" << stubPath;
            emit finished(false, "No se encontró el archivo 'loader_stub.exe'.");
            return false;
        }
    }
    
    // 6. Create self-extracting executable (if requested)
    bool createdSfx = false;
    
    if (config.generateSfx) {
        emit progress(90, "Creando ejecutable auto-extraíble...");
        
        // First, try to create a 7z archive
        QString sevenZipPath = distDir + ".7z";
    QFile::remove(sevenZipPath);
    
    QProcess sevenZipProcess;
    sevenZipProcess.setWorkingDirectory(config.outputPath);
    
    // Try to find 7z executable
    QString sevenZExe = QStandardPaths::findExecutable("7z");
    if (sevenZExe.isEmpty()) {
        sevenZExe = QStandardPaths::findExecutable("7za");
    }
    
    if (!sevenZExe.isEmpty()) {
        // Create 7z archive
        QStringList archiveArgs;
        archiveArgs << "a" << "-t7z" << "-mx=9" << baseName + "_win64.7z" << baseName + "_win64";
        
        sevenZipProcess.start(sevenZExe, archiveArgs);
        if (sevenZipProcess.waitForFinished(60000) && sevenZipProcess.exitCode() == 0) {
            qDebug() << "Created 7z archive";
            
            // Now try to create SFX
            // Look for 7z SFX module
            QString sfxModule;
            QStringList sfxPaths = {
                "/usr/lib/p7zip/7z.sfx",
                "/usr/lib/p7zip/7zCon.sfx",
                "/usr/share/p7zip/7z.sfx",
                runtimeDir + "/7zS.sfx",  // Custom location
                runtimeDir + "/7z.sfx"
            };
            
            for (const QString &path : sfxPaths) {
                if (QFile::exists(path)) {
                    sfxModule = path;
                    qDebug() << "Found SFX module:" << sfxModule;
                    break;
                }
            }
            
            if (!sfxModule.isEmpty()) {
                // Create config file for SFX
                QString sfxConfigPath = config.outputPath + "/sfx_config.txt";
                QFile sfxConfig(sfxConfigPath);
                if (sfxConfig.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream out(&sfxConfig);
                    out << ";!@Install@!UTF-8!\r\n";
                    out << "Title=\"" << project.name << "\"\r\n";
                    out << "BeginPrompt=\"¿Extraer " << project.name << "?\"\r\n";
                    out << "RunProgram=\"" << baseName << ".vbs\"\r\n";
                    out << "Directory=\"" << baseName << "\"\r\n";
                    out << ";!@InstallEnd@!\r\n";
                    sfxConfig.close();
                }
                
                // Combine SFX module + config + archive
                QString sfxExePath = config.outputPath + "/" + baseName + "_win64.exe";
                QFile::remove(sfxExePath);
                
                // Method 1: Using cat command (Linux)
                QProcess catProcess;
                QStringList catArgs;
                catArgs << sfxModule << sfxConfigPath << baseName + "_win64.7z";
                catProcess.setWorkingDirectory(config.outputPath);
                catProcess.setStandardOutputFile(sfxExePath);
                catProcess.start("cat", catArgs);
                
                if (catProcess.waitForFinished(30000) && QFile::exists(sfxExePath)) {
                    // Set executable permission
                    QFile::setPermissions(sfxExePath, 
                        QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                        QFile::ReadGroup | QFile::ExeGroup |
                        QFile::ReadOther | QFile::ExeOther);
                    
                    qDebug() << "Created SFX executable:" << sfxExePath;
                    createdSfx = true;
                    
                    // Clean up temporary files
                    QFile::remove(sfxConfigPath);
                    QFile::remove(sevenZipPath);
                } else {
                    qWarning() << "Failed to create SFX executable";
                }
            } else {
                qWarning() << "7z SFX module not found. Install p7zip-full package.";
            }
        }
    }
    }
    
    // 7. Create ZIP archive (if requested)
    if (config.generateZip) {
        if (!createdSfx && !createdStandalone) {
            emit progress(95, "Creando archivo ZIP...");
        }
        
        QString zipPath = config.outputPath + "/" + baseName + "_win64.zip";
        QFile::remove(zipPath);
        
        QProcess zipProcess;
        zipProcess.setWorkingDirectory(config.outputPath);
        
        QStringList zipArgs;
        zipArgs << "-r" << baseName + "_win64.zip" << baseName + "_win64";
        
        zipProcess.start("zip", zipArgs);
    if (zipProcess.waitForFinished(30000)) {
        if (zipProcess.exitCode() == 0) {
            qDebug() << "Created ZIP archive:" << zipPath;
            
            QString message = "Publicación Windows completada.\n\n";
            if (createdStandalone) {
                message += "✓ Ejecutable autónomo: " + baseName + ".exe\n";
                message += "  (Un solo archivo .exe con todo embebido)\n\n";
            }
            if (createdSfx) {
                message += "✓ Auto-extraíble: " + baseName + "_win64.exe\n";
            }
            message += "✓ ZIP: " + zipPath + "\n";
            message += "✓ Carpeta: " + distDir;
            
            emit progress(100, message);
        } else {
            qWarning() << "ZIP creation failed, but folder is ready:" << distDir;
            
            QString message = "Publicación Windows completada.\n\n";
            if (createdStandalone) {
                message += "✓ Ejecutable autónomo: " + baseName + ".exe\n";
            }
            message += "✓ Carpeta: " + distDir;
            
            emit progress(100, message);
        }
    } else {
        qWarning() << "ZIP command not available, but folder is ready:" << distDir;
        
        QString message = "Publicación Windows completada.\n\n";
        if (createdStandalone) {
            message += "✓ Ejecutable autónomo: " + baseName + ".exe\n";
        }
        if (createdSfx) {
            message += "✓ Auto-extraíble: " + baseName + "_win64.exe\n";
        }
        message += "✓ Carpeta: " + distDir;
        
        emit progress(100, message);
    }
    }
    
    return true;
}

bool Publisher::publishSwitch(const ProjectData &project, const PublishConfig &config)
{
    emit progress(10, "Preparando entorno Switch...");
    
    QString baseName = project.name.simplified().replace(" ", "_");
    QString distDir = config.outputPath + "/" + baseName;
    
    // Clean previous
    if (QDir(distDir).exists()) QDir(distDir).removeRecursively();
    QDir().mkpath(distDir);
    
    // Find Runtime (bgdi.elf)
    emit progress(20, "Buscando runtime (bgdi.elf)...");
    QString bgdiPath;
    
    // Prefer "runtime/switch" relative to app
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    candidates << appDir + "/runtime/switch/bgdi.elf"
               << appDir + "/../runtime/switch/bgdi.elf"
               << appDir + "/bgdi.elf"
               << QDir::currentPath() + "/bgdi.elf";
               
    for(const QString &c : candidates) {
        if (QFile::exists(c)) {
            bgdiPath = c;
            break;
        }
    }
    
    if (bgdiPath.isEmpty()) {
        emit finished(false, "No se encontró bgdi.elf para Switch.\n"
                             "Asegúrate de haber compilado para Switch y que el archivo esté en 'runtime/switch/'.");
        return false;
    }
    
    // Find Tools
    emit progress(30, "Buscando herramientas (nacptool, elf2nro)...");
    QString nacptool = "nacptool";
    QString elf2nro = "elf2nro";
    
    // Check DEVKITPRO env first (as baseline)
    QString devkitPro = qgetenv("DEVKITPRO");
    if (!devkitPro.isEmpty()) {
        QString toolsBin = devkitPro + "/tools/bin";
        if (QFile::exists(toolsBin + "/nacptool")) nacptool = toolsBin + "/nacptool";
        if (QFile::exists(toolsBin + "/elf2nro")) elf2nro = toolsBin + "/elf2nro";
    }
    
    // Check local runtime tools overrides (Priority)
    QStringList searchPaths;
    searchPaths << appDir 
                << appDir + "/tools"
                << appDir + "/runtime/switch" 
                << appDir + "/runtime/switch/tools";

    for (const QString &path : searchPaths) {
        if (QFile::exists(path + "/nacptool")) nacptool = path + "/nacptool";
        if (QFile::exists(path + "/elf2nro")) elf2nro = path + "/elf2nro";
    }
    
    // Prepare RomFS directory for embedding
    QString romfsDir = distDir + "/romfs";
    QDir().mkpath(romfsDir);

    // 1. Copy Compiled Game (.dcb)
    emit progress(40, "Copiando juego compilado al RomFS...");
    
    // Use project main script base name for DCB
    QFileInfo scriptInfo(project.path + "/" + project.mainScript);
    QString dcbName = scriptInfo.baseName() + ".dcb";
    QString sourceDcbPath = scriptInfo.absolutePath() + "/" + dcbName;
    
    // Fallback if not found (maybe not compiled yet?)
    if (!QFile::exists(sourceDcbPath)) {
        emit finished(false, "No se encontró el archivo compilado (.dcb).\nCompila el proyecto antes de publicar.");
        return false;
    }
    
    // Copy as game.dcb (Switch standard) inside romfs
    QFile::copy(sourceDcbPath, romfsDir + "/game.dcb");

    // 2. Copy Assets
    emit progress(50, "Copiando assets al RomFS...");
    // Copy into romfs/assets
    QDir assetsDest(romfsDir + "/assets");
    assetsDest.mkpath(".");
    copyDir(project.path + "/assets", romfsDir + "/assets");

    // 3. Generate NACP (Control file)
    emit progress(70, "Generando metadatos (NACP)...");
    QString nacpFile = distDir + "/control.nacp";
    QString version = !project.version.isEmpty() ? project.version : "1.0.0";
    
    QProcess nacpProc;
    nacpProc.setWorkingDirectory(distDir);
    
    // nacptool --create "Title" "Author" "Version" control.nacp
    QStringList nacpArgs;
    nacpArgs << "--create" << project.name << config.switchAuthor << version << "control.nacp";
    
    nacpProc.start(nacptool, nacpArgs);
    if (!nacpProc.waitForFinished() || nacpProc.exitCode() != 0) {
        qWarning() << "nacptool failed or not found:" << nacpProc.readAllStandardError();
    }

    // 4. Generate NRO
    emit progress(80, "Generando NRO (embebido)...");
    QString nroFile = distDir + "/" + baseName + ".nro";
    
    QStringList nroArgs;
    nroArgs << bgdiPath << nroFile;
    if (QFile::exists(nacpFile)) nroArgs << "--nacp=" + nacpFile;
    
    if (!config.iconPath.isEmpty() && QFile::exists(config.iconPath)) {
         nroArgs << "--icon=" + config.iconPath;
    }
    
    // RomFS embedding
    nroArgs << "--romfsdir=" + romfsDir;
    
    QProcess nroProc;
    nroProc.start(elf2nro, nroArgs);
    if (!nroProc.waitForFinished() || nroProc.exitCode() != 0) {
        QString err = nroProc.readAllStandardError();
        emit finished(false, "Error al generar ejecutable NRO:\n" + err + 
                             "\n\nVerifica que 'elf2nro' está en el PATH o en devkitPro/tools/bin.\n" +
                             "Verifica que el icono es formato soportado (JPG 256x256 por lo general).");
        return false;
    }
    
    // Cleanup temporary files
    QFile::remove(nacpFile);
    QDir(romfsDir).removeRecursively(); // Clean romfs source

    emit progress(100, "¡Publicación Switch completada!");
    QDesktopServices::openUrl(QUrl::fromLocalFile(distDir));
    return true;
}

bool Publisher::publishWeb(const ProjectData &project, const PublishConfig &config)
{
    emit progress(10, "Preparando entorno Web...");
    
    QString baseName = project.name.simplified().replace(" ", "_");
    QString distDir = config.outputPath + "/web_" + baseName;
    
    // Clean
    if (QDir(distDir).exists()) QDir(distDir).removeRecursively();
    QDir().mkpath(distDir);
    
    // 1. Find Runtime (bgdi.js, bgdi.wasm, bgdi.html)
    emit progress(20, "Buscando runtime Web...");
    QString appDir = QCoreApplication::applicationDirPath();
    QString webRuntime = appDir + "/runtime/web";
    
    if (!QFile::exists(webRuntime + "/bgdi.wasm")) {
        // Fallback relative to project for dev
        webRuntime = appDir + "/../runtime/web";
    }
    
    if (!QFile::exists(webRuntime + "/bgdi.wasm")) {
        emit finished(false, "No se encontró el runtime web (bgdi.wasm, bgdi.js).\nVerifica la carpeta runtime/web/.");
        return false;
    }
    
    // Copy runtime files
    QFile::copy(webRuntime + "/bgdi.wasm", distDir + "/bgdi.wasm");
    QFile::copy(webRuntime + "/bgdi.js", distDir + "/bgdi.js");
    
    // HTML Template
    QString htmlSource = webRuntime + "/bgdi.html";
    if (QFile::exists(htmlSource)) {
        QFile::copy(htmlSource, distDir + "/index.html");
    } else {
        emit finished(false, "Falta bgdi.html en runtime/web/.");
        return false;
    }

    // 2. Compile DCB
    emit progress(30, "Compilando juego...");
    QFileInfo scriptInfo(project.path + "/" + project.mainScript);
    QString dcbName = "game.dcb"; // Always use game.dcb for web loader convenience
    QString sourceDcbPath = scriptInfo.absolutePath() + "/" + scriptInfo.baseName() + ".dcb";
    
    // Check if compiled (we are the publisher, assume calling code ensured compilation or we check)
    if (!QFile::exists(sourceDcbPath)) {
         emit finished(false, "El juego no está compilado (.dcb no encontrado).\nCompila el proyecto antes de publicar.");
         return false;
    }
    
    // 3. Prepare Assets for Packaging
    emit progress(40, "Preparando assets...");
    QString dataSrcDir = config.outputPath + "/_web_data_tmp";
    if (QDir(dataSrcDir).exists()) QDir(dataSrcDir).removeRecursively();
    QDir().mkpath(dataSrcDir);
    
    // Copy dcb as game.dcb
    QFile::copy(sourceDcbPath, dataSrcDir + "/game.dcb");
    
    // Copy assets
    copyDir(project.path + "/assets", dataSrcDir + "/assets");
    
    // 4. Run file_packager.py
    emit progress(60, "Empaquetando assets (file_packager)...");
    
    QString python = "python3"; 
    // Check python
    QProcess checkPy;
    checkPy.start("python3", QStringList() << "--version");
    if (!checkPy.waitForFinished() || checkPy.exitCode() != 0) python = "python";
    
    // Find file_packager.py
    // Priority: 1. runtime/web/tools, 2. EMSDK
    QString packagerScript;
    if (QFile::exists(webRuntime + "/tools/file_packager.py")) {
        packagerScript = webRuntime + "/tools/file_packager.py";
    } else if (!config.emsdkPath.isEmpty()) {
        QStringList candidates;
        candidates << "/upstream/emscripten/tools/file_packager.py"
                   << "/upstream/emscripten/file_packager.py"
                   << "/fastcomp/emscripten/tools/file_packager.py";
                   
        for(const QString &c : candidates) {
            if (QFile::exists(config.emsdkPath + c)) {
                packagerScript = config.emsdkPath + c;
                break;
            }
        }
    }
    
    if (packagerScript.isEmpty()) {
        emit finished(false, "No se encontró 'file_packager.py'.\nInstala EMSDK desde el diálogo o pon la herramienta en 'runtime/web/tools/'.");
        QDir(dataSrcDir).removeRecursively();
        return false;
    }
    
    // Arguments: file_packager.py game.data --preload source@/ --js-output=game.data.js --no-heap-copy
    QString dataFile = distDir + "/game.data";
    QString jsDataFile = distDir + "/game.data.js";
    
    QStringList args;
    args << packagerScript << dataFile 
         << "--preload" << dataSrcDir + "@/" // Map dataSrcDir root to emscripten root /
         << "--js-output=" + jsDataFile
         << "--no-heap-copy";
         
    QProcess packager;
    packager.setWorkingDirectory(distDir);
    packager.start(python, args);
    
    if (!packager.waitForFinished() || packager.exitCode() != 0) {
        QString err = packager.readAllStandardError();
        emit finished(false, "Error ejecutando file_packager.py:\n" + err); 
        QDir(dataSrcDir).removeRecursively();
        return false;
    }
    
    // Cleanup tmp data
    QDir(dataSrcDir).removeRecursively();
    
    // 5. Update HTML Title
    emit progress(90, "Finalizando HTML...");
    QFile html(distDir + "/index.html");
    if (html.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QString content = html.readAll();
        content.replace("BennuGD Web Game", config.webTitle);
        content.replace("{{TITLE}}", config.webTitle);
        
        // Inject script tag for game.data.js if not present
        if (!content.contains("game.data.js")) {
             content.replace("</body>", "<script src=\"game.data.js\"></script>\n</body>");
        }
        
        html.seek(0);
        html.write(content.toUtf8());
        html.resize(html.pos());
        html.close();
    }

    emit progress(100, "¡Publicación Web completada!");
    QDesktopServices::openUrl(QUrl::fromLocalFile(distDir));
    emit finished(true, "Publicación Web completada exitosamente.\nOUTPUT:" + distDir);
    return true;
}

bool Publisher::copyDir(const QString &source, const QString &destination)
{
    QDir srcDir(source);
    if (!srcDir.exists()) return false;
    
    QDir destDir(destination);
    if (!destDir.exists()) destDir.mkpath(".");
    
    bool success = true;
    for (const QFileInfo &info : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString srcPath = info.absoluteFilePath();
        QString destPath = destDir.filePath(info.fileName());
        
        if (info.isDir()) {
            success &= copyDir(srcPath, destPath);
        } else {
            QFile::remove(destPath); // Overwrite
            success &= QFile::copy(srcPath, destPath);
        }
    }
    return success;
}
