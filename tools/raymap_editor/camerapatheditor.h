#ifndef CAMERAPATHEDITOR_H
#define CAMERAPATHEDITOR_H

#include <QDialog>
#include <QTimer>
#include "camerapath.h"
#include "camerapathcanvas.h"
#include "mapdata.h"

class QListWidget;
class QDoubleSpinBox;
class QSpinBox;
class QSlider;
class QComboBox;
class QPushButton;
class QLabel;
class QGraphicsView;
class QGraphicsScene;
class VisualModeWidget;

class CameraPathEditor : public QDialog
{
    Q_OBJECT

public:
    explicit CameraPathEditor(const MapData &mapData, QWidget *parent = nullptr);
    ~CameraPathEditor();

private slots:
    void onAddKeyframe();
    void onRemoveKeyframe();
    void onKeyframeSelected(int index);
    void onKeyframePropertyChanged();
    void onTimelineChanged(int value);
    void onPlayClicked();
    void onStopClicked();
    void onSaveClicked();
    void onLoadClicked();
    void onAnimationTick();
    void onCanvasKeyframeAdded(float x, float y);
    void onCanvasKeyframeSelected(int index);
    void onCanvasKeyframeMoved(int index, float x, float y);

private:
    void setupUI();
    void updateKeyframeList();
    void updateKeyframeProperties();
    void updateTimeline();
    void update3DPreview(float time);
    
    MapData m_mapData;
    CameraPath m_path;
    int m_selectedKeyframe;
    
    // UI Components
    CameraPathCanvas *m_2dView;
    VisualModeWidget *m_3dView;
    
    QListWidget *m_keyframeList;
    QDoubleSpinBox *m_posXSpin;
    QDoubleSpinBox *m_posYSpin;
    QDoubleSpinBox *m_posZSpin;
    QDoubleSpinBox *m_yawSpin;
    QDoubleSpinBox *m_pitchSpin;
    QDoubleSpinBox *m_rollSpin;
    QDoubleSpinBox *m_fovSpin;
    QDoubleSpinBox *m_timeSpin;
    QDoubleSpinBox *m_durationSpin;
    QDoubleSpinBox *m_speedSpin;
    QComboBox *m_easeInCombo;
    QComboBox *m_easeOutCombo;
    
    QSlider *m_timelineSlider;
    QLabel *m_timeLabel;
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    
    // Animation
    QTimer *m_animationTimer;
    float m_currentTime;
    bool m_isPlaying;
};

#endif // CAMERAPATHEDITOR_H
