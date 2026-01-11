#include "camerapatheditor.h"
#include "camerapathio.h"
#include "visualmodewidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QListWidget>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QFileDialog>
#include <QMessageBox>
#include <QWheelEvent>

CameraPathEditor::CameraPathEditor(const MapData &mapData, QWidget *parent)
    : QDialog(parent)
    , m_mapData(mapData)
    , m_selectedKeyframe(-1)
    , m_animationTimer(new QTimer(this))
    , m_currentTime(0.0f)
    , m_isPlaying(false)
{
    setupUI();
    setWindowTitle(tr("Editor de Cámaras Cinemáticas"));
    resize(1400, 800);
    
    m_path.setName("Nueva Secuencia");
    
    connect(m_animationTimer, &QTimer::timeout, this, &CameraPathEditor::onAnimationTick);
}

CameraPathEditor::~CameraPathEditor()
{
}

void CameraPathEditor::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    // Left: 2D View
    QVBoxLayout *leftLayout = new QVBoxLayout();
    QGroupBox *view2DGroup = new QGroupBox(tr("Vista 2D (Top-Down)"));
    QVBoxLayout *view2DLayout = new QVBoxLayout(view2DGroup);
    
    m_2dView = new CameraPathCanvas(this);
    m_2dView->setMapData(m_mapData);
    m_2dView->setCameraPath(&m_path);
    m_2dView->setMinimumSize(500, 400);
    view2DLayout->addWidget(m_2dView);
    
    // Connect canvas signals
    connect(m_2dView, &CameraPathCanvas::keyframeAdded,
            this, &CameraPathEditor::onCanvasKeyframeAdded);
    connect(m_2dView, &CameraPathCanvas::keyframeSelected,
            this, &CameraPathEditor::onCanvasKeyframeSelected);
    connect(m_2dView, &CameraPathCanvas::keyframeMoved,
            this, &CameraPathEditor::onCanvasKeyframeMoved);
    
    // Tools
    QHBoxLayout *toolsLayout = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(tr("➕ Añadir Keyframe"));
    connect(addBtn, &QPushButton::clicked, this, &CameraPathEditor::onAddKeyframe);
    toolsLayout->addWidget(addBtn);
    
    QPushButton *removeBtn = new QPushButton(tr("🗑️ Eliminar"));
    connect(removeBtn, &QPushButton::clicked, this, &CameraPathEditor::onRemoveKeyframe);
    toolsLayout->addWidget(removeBtn);
    
    view2DLayout->addLayout(toolsLayout);
    leftLayout->addWidget(view2DGroup);
    
    // Keyframe list
    QGroupBox *keyframesGroup = new QGroupBox(tr("Keyframes"));
    QVBoxLayout *keyframesLayout = new QVBoxLayout(keyframesGroup);
    m_keyframeList = new QListWidget();
    m_keyframeList->setSelectionMode(QAbstractItemView::ExtendedSelection); // Enable multi-selection
    connect(m_keyframeList, &QListWidget::currentRowChanged, this, &CameraPathEditor::onKeyframeSelected);
    keyframesLayout->addWidget(m_keyframeList);
    leftLayout->addWidget(keyframesGroup);
    
    mainLayout->addLayout(leftLayout);
    
    // Center: 3D View
    QVBoxLayout *centerLayout = new QVBoxLayout();
    QGroupBox *view3DGroup = new QGroupBox(tr("Vista 3D (Preview)"));
    QVBoxLayout *view3DLayout = new QVBoxLayout(view3DGroup);
    
    m_3dView = new VisualModeWidget(this);
    m_3dView->setMapData(m_mapData, true);
    m_3dView->setMinimumSize(500, 400);
    view3DLayout->addWidget(m_3dView);
    
    // Timeline
    QGroupBox *timelineGroup = new QGroupBox(tr("Timeline"));
    QVBoxLayout *timelineLayout = new QVBoxLayout(timelineGroup);
    
    m_timelineSlider = new QSlider(Qt::Horizontal);
    m_timelineSlider->setRange(0, 1000);
    connect(m_timelineSlider, &QSlider::valueChanged, this, &CameraPathEditor::onTimelineChanged);
    timelineLayout->addWidget(m_timelineSlider);
    
    m_timeLabel = new QLabel(tr("Tiempo: 0.0s / 0.0s"));
    timelineLayout->addWidget(m_timeLabel);
    
    QHBoxLayout *playbackLayout = new QHBoxLayout();
    m_playButton = new QPushButton(tr("▶ Reproducir"));
    connect(m_playButton, &QPushButton::clicked, this, &CameraPathEditor::onPlayClicked);
    playbackLayout->addWidget(m_playButton);
    
    m_stopButton = new QPushButton(tr("⏹ Detener"));
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &CameraPathEditor::onStopClicked);
    playbackLayout->addWidget(m_stopButton);
    
    timelineLayout->addLayout(playbackLayout);
    timelineGroup->setLayout(timelineLayout);
    
    view3DLayout->addWidget(timelineGroup);
    centerLayout->addWidget(view3DGroup);
    
    // File operations
    QHBoxLayout *fileLayout = new QHBoxLayout();
    QPushButton *saveBtn = new QPushButton(tr("💾 Guardar"));
    connect(saveBtn, &QPushButton::clicked, this, &CameraPathEditor::onSaveClicked);
    fileLayout->addWidget(saveBtn);
    
    QPushButton *loadBtn = new QPushButton(tr("📂 Cargar"));
    connect(loadBtn, &QPushButton::clicked, this, &CameraPathEditor::onLoadClicked);
    fileLayout->addWidget(loadBtn);
    
    QPushButton *closeBtn = new QPushButton(tr("Cerrar"));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    fileLayout->addWidget(closeBtn);
    
    centerLayout->addLayout(fileLayout);
    mainLayout->addLayout(centerLayout);
    
    // Right: Properties
    QVBoxLayout *rightLayout = new QVBoxLayout();
    QGroupBox *propsGroup = new QGroupBox(tr("Propiedades del Keyframe"));
    QFormLayout *propsLayout = new QFormLayout(propsGroup);
    
    m_posXSpin = new QDoubleSpinBox();
    m_posXSpin->setRange(-10000, 10000);
    connect(m_posXSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Posición X:"), m_posXSpin);
    
    m_posYSpin = new QDoubleSpinBox();
    m_posYSpin->setRange(-10000, 10000);
    connect(m_posYSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Posición Y:"), m_posYSpin);
    
    m_posZSpin = new QDoubleSpinBox();
    m_posZSpin->setRange(0, 1000);
    m_posZSpin->setValue(64);
    connect(m_posZSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Posición Z:"), m_posZSpin);
    
    m_yawSpin = new QDoubleSpinBox();
    m_yawSpin->setRange(-180, 180);
    m_yawSpin->setSuffix("°");
    connect(m_yawSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Yaw (Rotación):"), m_yawSpin);
    
    m_pitchSpin = new QDoubleSpinBox();
    m_pitchSpin->setRange(-90, 90);
    m_pitchSpin->setSuffix("°");
    connect(m_pitchSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Pitch (Inclinación):"), m_pitchSpin);
    
    m_rollSpin = new QDoubleSpinBox();
    m_rollSpin->setRange(-180, 180);
    m_rollSpin->setSuffix("°");
    connect(m_rollSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Roll:"), m_rollSpin);
    
    m_fovSpin = new QDoubleSpinBox();
    m_fovSpin->setRange(30, 120);
    m_fovSpin->setValue(90);
    m_fovSpin->setSuffix("°");
    connect(m_fovSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("FOV:"), m_fovSpin);
    
    m_timeSpin = new QDoubleSpinBox();
    m_timeSpin->setRange(0, 1000);
    m_timeSpin->setSuffix("s");
    connect(m_timeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Tiempo:"), m_timeSpin);
    
    m_durationSpin = new QDoubleSpinBox();
    m_durationSpin->setRange(0, 60);
    m_durationSpin->setSuffix("s");
    connect(m_durationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Duración (pausa):"), m_durationSpin);
    
    m_speedSpin = new QDoubleSpinBox();
    m_speedSpin->setRange(0.1, 10.0);
    m_speedSpin->setValue(1.0);
    m_speedSpin->setSingleStep(0.1);
    connect(m_speedSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Velocidad:"), m_speedSpin);
    
    m_easeInCombo = new QComboBox();
    for (int i = 0; i < 7; i++) {
        m_easeInCombo->addItem(easeTypeToString((EaseType)i));
    }
    connect(m_easeInCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Ease In:"), m_easeInCombo);
    
    m_easeOutCombo = new QComboBox();
    for (int i = 0; i < 7; i++) {
        m_easeOutCombo->addItem(easeTypeToString((EaseType)i));
    }
    connect(m_easeOutCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CameraPathEditor::onKeyframePropertyChanged);
    propsLayout->addRow(tr("Ease Out:"), m_easeOutCombo);
    
    rightLayout->addWidget(propsGroup);
    rightLayout->addStretch();
    
    mainLayout->addLayout(rightLayout);
}

void CameraPathEditor::onAddKeyframe()
{
    CameraKeyframe kf;
    kf.x = 0;
    kf.y = 0;
    kf.z = 64;
    kf.time = m_path.keyframeCount() > 0 ? m_path.totalDuration() + 1.0f : 0.0f;
    
    m_path.addKeyframe(kf);
    updateKeyframeList();
    updateTimeline();
}

void CameraPathEditor::onRemoveKeyframe()
{
    QList<QListWidgetItem*> selected = m_keyframeList->selectedItems();
    if (selected.isEmpty()) return;
    
    // Get indices and sort in reverse order to delete from end
    QList<int> indices;
    for (QListWidgetItem *item : selected) {
        indices.append(m_keyframeList->row(item));
    }
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    
    // Delete keyframes from end to start
    for (int index : indices) {
        m_path.removeKeyframe(index);
    }
    
    m_selectedKeyframe = -1;
    updateKeyframeList();
    updateTimeline();
    m_2dView->update();
}

void CameraPathEditor::onKeyframeSelected(int index)
{
    m_selectedKeyframe = index;
    updateKeyframeProperties();
}

void CameraPathEditor::onKeyframePropertyChanged()
{
    if (m_selectedKeyframe < 0) return;
    
    CameraKeyframe kf = m_path.getKeyframe(m_selectedKeyframe);
    kf.x = m_posXSpin->value();
    kf.y = m_posYSpin->value();
    kf.z = m_posZSpin->value();
    kf.yaw = m_yawSpin->value();
    kf.pitch = m_pitchSpin->value();
    kf.roll = m_rollSpin->value();
    kf.fov = m_fovSpin->value();
    kf.time = m_timeSpin->value();
    kf.duration = m_durationSpin->value();
    kf.speedMultiplier = m_speedSpin->value();
    kf.easeIn = (EaseType)m_easeInCombo->currentIndex();
    kf.easeOut = (EaseType)m_easeOutCombo->currentIndex();
    
    m_path.updateKeyframe(m_selectedKeyframe, kf);
    updateTimeline();
    m_2dView->update();
    
    // Update 3D preview in real-time
    update3DPreview(kf.time);
}

void CameraPathEditor::onTimelineChanged(int value)
{
    float time = (value / 1000.0f) * m_path.totalDuration();
    m_currentTime = time;
    m_timeLabel->setText(tr("Tiempo: %1s / %2s")
                         .arg(time, 0, 'f', 1)
                         .arg(m_path.totalDuration(), 0, 'f', 1));
    
    if (!m_isPlaying) {
        update3DPreview(time);
    }
}

void CameraPathEditor::onPlayClicked()
{
    if (m_isPlaying) {
        m_animationTimer->stop();
        m_isPlaying = false;
        m_playButton->setText(tr("▶ Reproducir"));
        m_stopButton->setEnabled(false);
    } else {
        m_animationTimer->start(16); // ~60 FPS
        m_isPlaying = true;
        m_playButton->setText(tr("⏸ Pausar"));
        m_stopButton->setEnabled(true);
    }
}

void CameraPathEditor::onStopClicked()
{
    m_animationTimer->stop();
    m_isPlaying = false;
    m_currentTime = 0.0f;
    m_timelineSlider->setValue(0);
    m_playButton->setText(tr("▶ Reproducir"));
    m_stopButton->setEnabled(false);
    update3DPreview(0.0f);
}

void CameraPathEditor::onSaveClicked()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Guardar Secuencia de Cámara"),
        "",
        tr("Camera Path (*.campath)"));
    
    if (filename.isEmpty()) return;
    
    if (!filename.endsWith(".campath", Qt::CaseInsensitive)) {
        filename += ".campath";
    }
    
    if (CameraPathIO::save(m_path, filename)) {
        QMessageBox::information(this, tr("Éxito"),
            tr("Secuencia guardada correctamente"));
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Error al guardar la secuencia"));
    }
}

void CameraPathEditor::onLoadClicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Cargar Secuencia de Cámara"),
        "",
        tr("Camera Path (*.campath)"));
    
    if (filename.isEmpty()) return;
    
    bool ok;
    m_path = CameraPathIO::load(filename, &ok);
    
    if (ok) {
        updateKeyframeList();
        updateTimeline();
        QMessageBox::information(this, tr("Éxito"),
            tr("Secuencia cargada correctamente"));
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Error al cargar la secuencia"));
    }
}

void CameraPathEditor::onAnimationTick()
{
    m_currentTime += 0.016f; // ~60 FPS
    
    if (m_currentTime > m_path.totalDuration()) {
        if (m_path.loop()) {
            m_currentTime = 0.0f;
        } else {
            onStopClicked();
            return;
        }
    }
    
    int sliderValue = (m_currentTime / m_path.totalDuration()) * 1000;
    m_timelineSlider->setValue(sliderValue);
    
    update3DPreview(m_currentTime);
}

void CameraPathEditor::updateKeyframeList()
{
    m_keyframeList->clear();
    for (int i = 0; i < m_path.keyframeCount(); i++) {
        CameraKeyframe kf = m_path.getKeyframe(i);
        m_keyframeList->addItem(tr("Keyframe %1 (t=%2s)")
                                .arg(i + 1)
                                .arg(kf.time, 0, 'f', 1));
    }
}

void CameraPathEditor::updateKeyframeProperties()
{
    if (m_selectedKeyframe < 0) return;
    
    CameraKeyframe kf = m_path.getKeyframe(m_selectedKeyframe);
    
    m_posXSpin->setValue(kf.x);
    m_posYSpin->setValue(kf.y);
    m_posZSpin->setValue(kf.z);
    m_yawSpin->setValue(kf.yaw);
    m_pitchSpin->setValue(kf.pitch);
    m_rollSpin->setValue(kf.roll);
    m_fovSpin->setValue(kf.fov);
    m_timeSpin->setValue(kf.time);
    m_durationSpin->setValue(kf.duration);
    m_speedSpin->setValue(kf.speedMultiplier);
    m_easeInCombo->setCurrentIndex((int)kf.easeIn);
    m_easeOutCombo->setCurrentIndex((int)kf.easeOut);
}

void CameraPathEditor::updateTimeline()
{
    m_timeLabel->setText(tr("Tiempo: 0.0s / %1s")
                         .arg(m_path.totalDuration(), 0, 'f', 1));
}

void CameraPathEditor::update3DPreview(float time)
{
    if (m_path.keyframeCount() == 0) return;
    
    CameraKeyframe kf = m_path.interpolateAt(time);
    
    // Update 3D view camera
    // IMPORTANT: Coordinate system conversion:
    // 2D Map: X=horizontal, Y=vertical (depth)
    // 3D View: X=horizontal, Y=height, Z=depth
    m_3dView->setCameraPosition(kf.x, kf.z, kf.y);  // Swap Y and Z!
    m_3dView->setCameraRotation(kf.yaw * M_PI / 180.0f, kf.pitch * M_PI / 180.0f);  // Convert to radians
    m_3dView->update();
}

void CameraPathEditor::onCanvasKeyframeAdded(float x, float y)
{
    CameraKeyframe kf;
    kf.x = x;
    kf.y = y;
    kf.z = 64;
    kf.time = m_path.keyframeCount() > 0 ? m_path.totalDuration() + 1.0f : 0.0f;
    
    m_path.addKeyframe(kf);
    updateKeyframeList();
    updateTimeline();
    m_2dView->update();
}

void CameraPathEditor::onCanvasKeyframeSelected(int index)
{
    m_selectedKeyframe = index;
    m_keyframeList->setCurrentRow(index);
    updateKeyframeProperties();
    m_2dView->setSelectedKeyframe(index);
}

void CameraPathEditor::onCanvasKeyframeMoved(int index, float x, float y)
{
    if (index < 0 || index >= m_path.keyframeCount()) return;
    
    CameraKeyframe kf = m_path.getKeyframe(index);
    kf.x = x;
    kf.y = y;
    m_path.updateKeyframe(index, kf);
    
    // Update UI if this is the selected keyframe
    if (index == m_selectedKeyframe) {
        m_posXSpin->setValue(x);
        m_posYSpin->setValue(y);
    }
    
    m_2dView->update();
}
