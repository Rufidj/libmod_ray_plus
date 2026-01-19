#include "publishdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QStackedWidget>
#include <QGroupBox>
#include <QTimer>
#include <QDebug>
#include <QDesktopServices>
#include <QUrl>

PublishDialog::PublishDialog(ProjectData *project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    setWindowTitle(tr("Publicar Proyecto"));
    resize(550, 450);
    setupUI();
    
    // Load saved settings
    if (m_project) {
        m_packageNameEdit->setText(m_project->packageName);
        if (m_packageNameEdit->text().isEmpty()) m_packageNameEdit->setText("com.example.game");
        
        m_iconPathEdit->setText(m_project->iconPath);
    }
}

void PublishDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Platform Selection
    QFormLayout *topLayout = new QFormLayout();
    m_platformCombo = new QComboBox();
    m_platformCombo->addItem(tr("Linux (64-bit)"), Publisher::Linux);
    // Android disabled - BennuGD2 doesn't support Android compilation yet
    // m_platformCombo->addItem(tr("Android"), Publisher::Android);
    
    topLayout->addRow(tr("Plataforma Destino:"), m_platformCombo);
    
    // Output Path
    QHBoxLayout *pathLayout = new QHBoxLayout();
    m_outputPathEdit = new QLineEdit();
    QPushButton *browseBtn = new QPushButton("...");
    pathLayout->addWidget(m_outputPathEdit);
    pathLayout->addWidget(browseBtn);
    topLayout->addRow(tr("Carpeta de Salida:"), pathLayout);
    
    mainLayout->addLayout(topLayout);

    // Stacked Options
    QStackedWidget *optionsStack = new QStackedWidget();
    
    // === LINUX OPTIONS ===
    m_linuxOptions = new QWidget();
    QVBoxLayout *linuxLayout = new QVBoxLayout(m_linuxOptions);
    QGroupBox *linuxGroup = new QGroupBox(tr("Opciones de Linux"));
    QVBoxLayout *linuxGroupLayout = new QVBoxLayout(linuxGroup);
    
    m_chkLinuxArchive = new QCheckBox(tr("Crear paquete comprimido (.tar.gz)"));
    m_chkLinuxArchive->setChecked(true);
    m_chkLinuxArchive->setToolTip(tr("Incluye ejecutable, librerías y script de lanzamiento"));
    
    m_chkLinuxAppImage = new QCheckBox(tr("Crear AppImage"));
    
    QHBoxLayout *appImageLayout = new QHBoxLayout();
    appImageLayout->addWidget(m_chkLinuxAppImage);
    
    QPushButton *dlAppImageBtn = new QPushButton(QIcon::fromTheme("download"), tr("Descargar Tool"));
    dlAppImageBtn->setToolTip(tr("Descargar appimagetool si no está instalado"));
    connect(dlAppImageBtn, &QPushButton::clicked, this, &PublishDialog::onDownloadAppImageTool);
    appImageLayout->addWidget(dlAppImageBtn);
    
    // Check availability
    QString systemTool = QStandardPaths::findExecutable("appimagetool");
    QString localTool = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/bin/appimagetool";
    
    if (!systemTool.isEmpty()) {
        m_appImageToolPath = systemTool;
    } else if (QFile::exists(localTool)) {
        m_appImageToolPath = localTool;
    }
    
    bool hasAppImageTool = !m_appImageToolPath.isEmpty();
    m_chkLinuxAppImage->setEnabled(hasAppImageTool);
    m_chkLinuxAppImage->setText(hasAppImageTool ? tr("Crear AppImage (Disponible)") : tr("Crear AppImage (Falta herramienta)"));
    dlAppImageBtn->setVisible(!hasAppImageTool);

    linuxGroupLayout->addLayout(appImageLayout);
    linuxLayout->addWidget(linuxGroup);
    linuxLayout->addStretch();
    
    // === ANDROID OPTIONS ===
    m_androidOptions = new QWidget();
    QVBoxLayout *androidLayout = new QVBoxLayout(m_androidOptions);
    QGroupBox *androidGroup = new QGroupBox(tr("Opciones de Android"));
    QFormLayout *androidForm = new QFormLayout(androidGroup);
    
    m_packageNameEdit = new QLineEdit();
    m_packageNameEdit->setPlaceholderText("com.company.game");
    
    m_iconPathEdit = new QLineEdit();
    QHBoxLayout *iconLayout = new QHBoxLayout();
    iconLayout->addWidget(m_iconPathEdit);
    QPushButton *iconBrowseBtn = new QPushButton("...");
    iconLayout->addWidget(iconBrowseBtn);
    
    m_chkAndroidProject = new QCheckBox(tr("Generar Proyecto Android Studio"));
    m_chkAndroidProject->setChecked(true);
    m_chkAndroidProject->setEnabled(false); // Always generate project
    
    m_chkAndroidAPK = new QCheckBox(tr("Intentar compilar APK (Requiere SDK/NDK)"));
    m_chkAndroidAPK->setChecked(false);
    
    QPushButton *ndkHelpBtn = new QPushButton(QIcon::fromTheme("download"), tr("Descargar NDK"));
    ndkHelpBtn->setToolTip(tr("Descargar e instalar NDK r26b")); // Recommended version
    connect(ndkHelpBtn, &QPushButton::clicked, this, &PublishDialog::onDownloadNDK);
    
    // Check if NDK exists
    QString ndkHome = qgetenv("ANDROID_NDK");
    if (ndkHome.isEmpty()) ndkHome = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Android/Sdk/ndk/27.0.12077973"; // Expected by BennuGD2
    
    bool hasNDK = QDir(ndkHome).exists();
    if (hasNDK) {
        ndkHelpBtn->setText(tr("NDK Detectado"));
        ndkHelpBtn->setEnabled(false);
        ndkHelpBtn->setIcon(QIcon::fromTheme("emblem-ok-symbolic"));
    }
    
    QHBoxLayout *apkLayout = new QHBoxLayout();
    apkLayout->addWidget(m_chkAndroidAPK);
    apkLayout->addWidget(ndkHelpBtn);
    apkLayout->addStretch();
    
    androidForm->addRow(tr("Package Name:"), m_packageNameEdit);
    androidForm->addRow(tr("Icono (.png):"), iconLayout);
    androidForm->addRow(m_chkAndroidProject);
    androidForm->addRow(apkLayout); // Layout with button
    
    androidLayout->addWidget(androidGroup);
    
    QLabel *androidInfo = new QLabel(tr("Nota: Se generará un proyecto completo con Gradle.\n"
                                        "El editor copiará las librerías si se encuentran."));
    androidInfo->setWordWrap(true);
    androidInfo->setStyleSheet("color: #888; font-style: italic;");
    androidLayout->addWidget(androidInfo);
    
    androidLayout->addStretch();
    
    optionsStack->addWidget(m_linuxOptions);
    optionsStack->addWidget(m_androidOptions);
    
    mainLayout->addWidget(optionsStack);

    // Progress Bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(true);
    mainLayout->addWidget(m_progressBar);

    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_closeButton = new QPushButton(tr("Cancelar"));
    m_publishButton = new QPushButton(tr("Publicar"));
    m_publishButton->setDefault(true);
    m_publishButton->setStyleSheet("font-weight: bold; padding: 5px 20px;");
    
    btnLayout->addStretch();
    btnLayout->addWidget(m_closeButton);
    btnLayout->addWidget(m_publishButton);
    mainLayout->addLayout(btnLayout);

    // Connections
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(browseBtn, &QPushButton::clicked, this, &PublishDialog::onBrowseOutput);
    connect(iconBrowseBtn, &QPushButton::clicked, this, &PublishDialog::onBrowseIcon);
    
    // Sync Combo with Stack
    connect(m_platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            optionsStack, &QStackedWidget::setCurrentIndex);
    connect(m_platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &PublishDialog::onPlatformChanged);

    connect(m_publishButton, &QPushButton::clicked, this, &PublishDialog::onPublish);
    
    // Publisher connections
    connect(&m_publisher, &Publisher::progress, m_progressBar, &QProgressBar::setValue);
    connect(&m_publisher, &Publisher::progress, [this](int, QString msg){
        m_progressBar->setFormat("%p% - " + msg);
    });
    
    connect(&m_publisher, &Publisher::finished, [this](bool success, QString msg){
        m_publishButton->setEnabled(true);
        m_closeButton->setEnabled(true);
        m_progressBar->setVisible(false);
        if (success) {
            QMessageBox::information(this, tr("Publicación Exitosa"), msg);
            accept();
        } else {
            QMessageBox::critical(this, tr("Error de Publicación"), msg);
        }
    });
}

void PublishDialog::onPublish()
{
    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Aviso"), tr("Por favor selecciona una carpeta de salida."));
        return;
    }

    // Save project metadata back to project data (to be persisted later)
    if (m_project) {
        m_project->packageName = m_packageNameEdit->text();
        m_project->iconPath = m_iconPathEdit->text();
    }

    Publisher::PublishConfig config;
    config.platform = (Publisher::Platform)m_platformCombo->currentData().toInt();
    config.outputPath = m_outputPathEdit->text();
    
    if (config.platform == Publisher::Linux) {
        config.generateAppImage = m_chkLinuxAppImage->isChecked();
        config.appImageToolPath = m_appImageToolPath;
    } else if (config.platform == Publisher::Android) {
        config.packageName = m_packageNameEdit->text();
        config.iconPath = m_iconPathEdit->text();
        config.fullProject = m_chkAndroidProject->isChecked();
        config.generateAPK = m_chkAndroidAPK->isChecked();
        
        // Use environment variable for NDK if set
        QString envNdk = qgetenv("ANDROID_NDK_HOME");
        if (!envNdk.isEmpty()) config.ndkPath = envNdk;
        
        if (config.packageName.isEmpty()) {
             QMessageBox::warning(this, tr("Aviso"), tr("El nombre de paquete es obligatorio para Android."));
             return;
        }
        
        // Basic validation
        QRegularExpression regex("^[a-z][a-z0-9_]*(\\.[a-z][a-z0-9_]*)+$");
        if (!regex.match(config.packageName).hasMatch()) {
             QMessageBox::warning(this, tr("Aviso"), tr("El nombre de paquete debe tener formato 'com.empresa.juego'."));
             return;
        }
    }
    
    m_publishButton->setEnabled(false);
    m_closeButton->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("Iniciando...");
    
    // Execute asynchronously to keep UI alive (simulated via timer for now)
    QTimer::singleShot(100, [this, config](){
        m_publisher.publish(*m_project, config);
    });
}

void PublishDialog::onDownloadAppImageTool()
{
    QString url = "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage";
    QString dest = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.local/bin/appimagetool";
    QDir().mkpath(QFileInfo(dest).path()); // Ensure ~/.local/bin exists
    
    DownloadDialog dlg(url, dest, tr("Descargando AppImageTool"), false, this);
    if (dlg.start()) {
        QMessageBox::information(this, tr("Éxito"), tr("Herramienta descargada en %1\nAsegúrate de que esta ruta esté en tu PATH.").arg(dest));
        m_appImageToolPath = dest; // Store path
        m_chkLinuxAppImage->setEnabled(true);
        m_chkLinuxAppImage->setText(tr("Crear AppImage (Disponible)"));
    }
}

void PublishDialog::onDownloadNDK()
{
    // NDK 27.0.12077973 (as expected by BennuGD2's build-android-deps.sh)
    QString url = "https://dl.google.com/android/repository/android-ndk-r27-linux.zip";
    QString androidSdk = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/Android/Sdk";
    QString destZip = androidSdk + "/ndk-bundle.zip";
    
    QDir().mkpath(androidSdk);
    
    // Use true for autoUnzip
    DownloadDialog dlg(url, destZip, tr("Descargando NDK 27 (1GB+)"), true, this);
    if (dlg.start()) {
        // Unzip creates "android-ndk-r27" folder inside androidSdk
        QString extractedPath = androidSdk + "/android-ndk-r27";
        
        // Create symlink to match expected path structure
        QString expectedPath = androidSdk + "/ndk/27.0.12077973";
        QDir().mkpath(androidSdk + "/ndk");
        
        // Remove old symlink if exists
        QFile::remove(expectedPath);
        
        // Create symlink (or copy if symlink fails)
        if (!QFile::link(extractedPath, expectedPath)) {
            // Fallback: just set env to extracted path
            qputenv("ANDROID_NDK", extractedPath.toUtf8());
        } else {
            qputenv("ANDROID_NDK", expectedPath.toUtf8());
        }
        
        QMessageBox::information(this, tr("NDK Instalado"), 
            tr("El NDK 27 se ha instalado en:\n%1\n\nSe ha configurado ANDROID_NDK para esta sesión.\n\nPara uso permanente, agrega a ~/.bashrc:\nexport ANDROID_NDK=%1").arg(extractedPath));
           
        // Refresh could be better but this is sufficient for now
    }
}

void PublishDialog::onBrowseOutput()
{
    QString initialDir = m_project ? m_project->path : QDir::homePath();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Seleccionar Carpeta de Salida"), 
                                                  initialDir);
    if (!dir.isEmpty()) {
        m_outputPathEdit->setText(dir);
    }
}

void PublishDialog::onBrowseIcon()
{
    QString initialDir = m_project ? m_project->path : QDir::homePath();
    QString file = QFileDialog::getOpenFileName(this, tr("Seleccionar Icono"), 
                                              initialDir, "Images (*.png)");
    if (!file.isEmpty()) {
        m_iconPathEdit->setText(file);
    }
}

void PublishDialog::onPlatformChanged(int index)
{
    // Update logic if needed
}
