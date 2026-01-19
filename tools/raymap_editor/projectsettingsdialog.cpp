#include "projectsettingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QDir>

ProjectSettingsDialog::ProjectSettingsDialog(const ProjectData &data, QWidget *parent)
    : QDialog(parent), m_data(data)
{
    setupUi();
    setWindowTitle(tr("Configuración del Proyecto"));
    resize(500, 600);
}

void ProjectSettingsDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // --- General Settings ---
    QGroupBox *generalGroup = new QGroupBox(tr("General"), this);
    QFormLayout *generalLayout = new QFormLayout(generalGroup);
    
    m_nameEdit = new QLineEdit(m_data.name);
    m_versionEdit = new QLineEdit(m_data.version);
    
    generalLayout->addRow(tr("Nombre del Proyecto:"), m_nameEdit);
    generalLayout->addRow(tr("Versión:"), m_versionEdit);
    
    mainLayout->addWidget(generalGroup);
    
    // --- Files ---
    QGroupBox *filesGroup = new QGroupBox(tr("Archivos Principales"), this);
    QFormLayout *filesLayout = new QFormLayout(filesGroup);
    
    // FPG
    QHBoxLayout *fpgLayout = new QHBoxLayout();
    m_fpgEdit = new QLineEdit(m_data.fpgFile);
    m_browseFpgBtn = new QPushButton(tr("..."));
    connect(m_browseFpgBtn, &QPushButton::clicked, this, &ProjectSettingsDialog::onBrowseFPG);
    fpgLayout->addWidget(m_fpgEdit);
    fpgLayout->addWidget(m_browseFpgBtn);
    filesLayout->addRow(tr("Archivo FPG:"), fpgLayout);
    
    // Initial Map
    QHBoxLayout *mapLayout = new QHBoxLayout();
    m_mapEdit = new QLineEdit(m_data.initialMap);
    m_browseMapBtn = new QPushButton(tr("..."));
    connect(m_browseMapBtn, &QPushButton::clicked, this, &ProjectSettingsDialog::onBrowseMap);
    mapLayout->addWidget(m_mapEdit);
    mapLayout->addWidget(m_browseMapBtn);
    filesLayout->addRow(tr("Mapa Inicial:"), mapLayout);
    
    mainLayout->addWidget(filesGroup);
    
    // --- Display ---
    QGroupBox *displayGroup = new QGroupBox(tr("Pantalla y Motor"), this);
    QGridLayout *displayLayout = new QGridLayout(displayGroup);
    
    m_widthSpin = new QSpinBox();
    m_widthSpin->setRange(320, 3840);
    m_widthSpin->setValue(m_data.screenWidth);
    
    m_heightSpin = new QSpinBox();
    m_heightSpin->setRange(200, 2160);
    m_heightSpin->setValue(m_data.screenHeight);
    
    m_fpsSpin = new QSpinBox();
    m_fpsSpin->setRange(0, 240); // 0 = Unlimited
    m_fpsSpin->setSpecialValueText(tr("Ilimitado"));
    m_fpsSpin->setValue(m_data.fps);
    
    m_fovSpin = new QSpinBox();
    m_fovSpin->setRange(60, 120);
    m_fovSpin->setValue(m_data.fov);
    
    displayLayout->addWidget(new QLabel(tr("Ancho:")), 0, 0);
    displayLayout->addWidget(m_widthSpin, 0, 1);
    displayLayout->addWidget(new QLabel(tr("Alto:")), 0, 2);
    displayLayout->addWidget(m_heightSpin, 0, 3);
    
    displayLayout->addWidget(new QLabel(tr("FPS:")), 1, 0);
    displayLayout->addWidget(m_fpsSpin, 1, 1);
    displayLayout->addWidget(new QLabel(tr("FOV:")), 1, 2);
    displayLayout->addWidget(m_fpsSpin, 1, 1);
    displayLayout->addWidget(new QLabel(tr("FOV:")), 1, 2);
    displayLayout->addWidget(m_fovSpin, 1, 3);
    
    m_qualitySpin = new QSpinBox();
    m_qualitySpin->setRange(1, 16);
    m_qualitySpin->setValue(m_data.raycastQuality);
    m_qualitySpin->setToolTip(tr("Calidad del Raycasting (1=Mejor, >1=Más rápido/Pixelado)"));
    
    displayLayout->addWidget(new QLabel(tr("Calidad:")), 2, 0);
    displayLayout->addWidget(m_qualitySpin, 2, 1);
    
    mainLayout->addWidget(displayGroup);
    
    // --- Initial Camera ---
    QGroupBox *cameraGroup = new QGroupBox(tr("Cámara Inicial"), this);
    QGridLayout *camLayout = new QGridLayout(cameraGroup);
    
    m_camX = new QDoubleSpinBox(); m_camX->setRange(-10000, 10000); m_camX->setValue(m_data.cameraX);
    m_camY = new QDoubleSpinBox(); m_camY->setRange(-10000, 10000); m_camY->setValue(m_data.cameraY);
    m_camZ = new QDoubleSpinBox(); m_camZ->setRange(-10000, 10000); m_camZ->setValue(m_data.cameraZ);
    m_camRot = new QDoubleSpinBox(); m_camRot->setRange(-360, 360); m_camRot->setValue(m_data.cameraRot);
    m_camPitch = new QDoubleSpinBox(); m_camPitch->setRange(-90, 90); m_camPitch->setValue(m_data.cameraPitch);
    
    camLayout->addWidget(new QLabel("X:"), 0, 0); camLayout->addWidget(m_camX, 0, 1);
    camLayout->addWidget(new QLabel("Y:"), 0, 2); camLayout->addWidget(m_camY, 0, 3);
    camLayout->addWidget(new QLabel("Z:"), 0, 4); camLayout->addWidget(m_camZ, 0, 5);
    
    camLayout->addWidget(new QLabel("Rotación:"), 1, 0); camLayout->addWidget(m_camRot, 1, 1);
    camLayout->addWidget(new QLabel("Pitch:"), 1, 2); camLayout->addWidget(m_camPitch, 1, 3);
    
    mainLayout->addWidget(cameraGroup);
    
    // Add stretch
    mainLayout->addStretch();
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &ProjectSettingsDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void ProjectSettingsDialog::onBrowseFPG()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Seleccionar FPG"), m_data.path, "FPG Files (*.fpg)");
    if (!path.isEmpty()) {
        // Make relative if inside project
        QDir projectDir(m_data.path);
        QString relativePath = projectDir.relativeFilePath(path);
        
        if (!relativePath.startsWith("..") && !QFileInfo(relativePath).isAbsolute()) {
            m_fpgEdit->setText(relativePath);
        } else {
            m_fpgEdit->setText(path);
        }
    }
}

void ProjectSettingsDialog::onBrowseMap()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Seleccionar Mapa Inicial"), m_data.path, "RayMap Files (*.raymap)");
    if (!path.isEmpty()) {
        QDir projectDir(m_data.path);
        QString relativePath = projectDir.relativeFilePath(path);
        
        if (!relativePath.startsWith("..") && !QFileInfo(relativePath).isAbsolute()) {
            m_mapEdit->setText(relativePath);
        } else {
            m_mapEdit->setText(path);
        }
    }
}

void ProjectSettingsDialog::onAccept()
{
    // Update m_data with UI values
    m_data.name = m_nameEdit->text();
    m_data.version = m_versionEdit->text();
    m_data.fpgFile = m_fpgEdit->text();
    m_data.initialMap = m_mapEdit->text();
    
    m_data.screenWidth = m_widthSpin->value();
    m_data.screenHeight = m_heightSpin->value();
    m_data.fps = m_fpsSpin->value();
    m_data.fps = m_fpsSpin->value();
    m_data.fov = m_fovSpin->value();
    m_data.raycastQuality = m_qualitySpin->value();
    
    m_data.cameraX = m_camX->value();
    m_data.cameraY = m_camY->value();
    m_data.cameraZ = m_camZ->value();
    m_data.cameraRot = m_camRot->value();
    m_data.cameraPitch = m_camPitch->value();
    
    accept();
}

ProjectData ProjectSettingsDialog::getProjectData() const
{
    return m_data;
}
