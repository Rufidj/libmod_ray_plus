#include "mainwindow.h"
#include "newprojectdialog.h"
#include "projectmanager.h"
#include "assetbrowser.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>

// ============================================================================
// PROJECT MANAGEMENT
// ============================================================================

void MainWindow::onNewProject()
{
    NewProjectDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getProjectName();
        QString path = dialog.getProjectPath();
        
        // Create ProjectManager if it doesn't exist
        if (!m_projectManager) {
            m_projectManager = new ProjectManager();
        }
        
        // Close current project if any
        if (m_projectManager->hasProject()) {
            onCloseProject();
        }
        
        // Create new project
        if (m_projectManager->createProject(path, name)) {
            QMessageBox::information(this, "Proyecto Creado",
                QString("Proyecto '%1' creado exitosamente en:\n%2")
                .arg(name)
                .arg(m_projectManager->getProjectPath()));
            
            if (m_assetBrowser) {
                m_assetBrowser->setProjectPath(m_projectManager->getProjectPath());
            }

            updateWindowTitle();
        } else {
            QMessageBox::warning(this, "Error",
                "No se pudo crear el proyecto.");
        }
    }
}

void MainWindow::onOpenProject()
{
    // Select project directory instead of file
    QString dirPath = QFileDialog::getExistingDirectory(
        this,
        "Seleccionar Carpeta del Proyecto BennuGD2",
        "",
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (dirPath.isEmpty()) {
        return;
    }
    
    // Find .bgd2proj file in the selected directory
    QDir dir(dirPath);
    QStringList projFiles = dir.entryList(QStringList() << "*.bgd2proj", QDir::Files);
    
    QString fileName;
    if (projFiles.isEmpty()) {
        QMessageBox::warning(this, "Error",
            "No se encontró ningún archivo .bgd2proj en la carpeta seleccionada.");
        return;
    } else if (projFiles.size() == 1) {
        fileName = dirPath + "/" + projFiles.first();
    } else {
        // Multiple .bgd2proj files, let user choose
        bool ok;
        QString selected = QInputDialog::getItem(this, "Seleccionar Proyecto",
            "Se encontraron múltiples proyectos. Selecciona uno:",
            projFiles, 0, false, &ok);
        if (ok && !selected.isEmpty()) {
            fileName = dirPath + "/" + selected;
        } else {
            return;
        }
    }
    
    // Create ProjectManager if it doesn't exist
    if (!m_projectManager) {
        m_projectManager = new ProjectManager();
    }
    
    // Close current project if any
    if (m_projectManager->hasProject()) {
        onCloseProject();
    }
    
    // Open project
    if (m_projectManager->openProject(fileName)) {
        QMessageBox::information(this, "Proyecto Abierto",
            QString("Proyecto '%1' abierto exitosamente.")
            .arg(m_projectManager->getProject()->name));
        
        if (m_assetBrowser) {
            m_assetBrowser->setProjectPath(m_projectManager->getProjectPath());
        }

        updateWindowTitle();
        
        // Auto-open maps from project's maps folder
        QString mapsDir = m_projectManager->getProjectPath() + "/assets/maps";
        QDir dir(mapsDir);
        if (dir.exists()) {
            QStringList filters;
            filters << "*.raymap" << "*.rmap";
            QFileInfoList mapFiles = dir.entryInfoList(filters, QDir::Files);
            
            for (const QFileInfo &mapFile : mapFiles) {
                qDebug() << "Auto-opening map:" << mapFile.absoluteFilePath();
                openMapFile(mapFile.absoluteFilePath());
            }
            
            if (!mapFiles.isEmpty()) {
                m_statusLabel->setText(tr("Proyecto cargado: %1 mapas abiertos")
                                      .arg(mapFiles.size()));
            }
        }
    } else {
        QMessageBox::warning(this, "Error",
            "No se pudo abrir el proyecto.");
    }
}

void MainWindow::onCloseProject()
{
    if (!m_projectManager || !m_projectManager->hasProject()) {
        QMessageBox::information(this, "Sin Proyecto",
            "No hay ningún proyecto abierto.");
        return;
    }
    
    // Ask for confirmation
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Cerrar Proyecto",
        QString("¿Cerrar el proyecto '%1'?")
        .arg(m_projectManager->getProject()->name),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_projectManager->closeProject();
        
        // Cleanup UI
        if (m_assetBrowser) {
             m_assetBrowser->setProjectPath(""); // Clear asset browser
        }
        
        // Close all tabs (except maybe a default empty one)
        m_tabWidget->clear();
        onNewMap(); // Open a default empty map
        
        if (m_consoleWidget) {
             m_consoleWidget->clear();
             m_consoleDock->hide();
        }
        
        updateWindowTitle();
        
        QMessageBox::information(this, "Proyecto Cerrado",
            "El proyecto se ha cerrado.");
    }
}

void MainWindow::onProjectSettings()
{
    if (!m_projectManager || !m_projectManager->hasProject()) {
        QMessageBox::information(this, "Sin Proyecto",
            "No hay ningún proyecto abierto.\nCrea o abre un proyecto primero.");
        return;
    }
    
    // TODO: Implement project settings dialog
    QMessageBox::information(this, "Configuración del Proyecto",
        "Configuración del proyecto (próximamente)");
}
