#ifndef CAMERAPATH_H
#define CAMERAPATH_H

#include "camerakeyframe.h"
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QPointF>
#include <QtMath>

enum class InterpolationType {
    LINEAR,
    CATMULL_ROM,
    BEZIER
};

class CameraPath
{
public:
    CameraPath();
    
    // Keyframe management
    void addKeyframe(const CameraKeyframe &kf);
    void removeKeyframe(int index);
    void updateKeyframe(int index, const CameraKeyframe &kf);
    CameraKeyframe getKeyframe(int index) const;
    int keyframeCount() const { return m_keyframes.size(); }
    
    // Path properties
    void setName(const QString &name) { m_name = name; }
    QString name() const { return m_name; }
    
    void setInterpolation(InterpolationType type) { m_interpolation = type; }
    InterpolationType interpolation() const { return m_interpolation; }
    
    void setLoop(bool loop) { m_loop = loop; }
    bool loop() const { return m_loop; }
    
    float totalDuration() const { return m_totalDuration; }
    
    // Interpolation
    CameraKeyframe interpolateAt(float time) const;
    QVector<QPointF> generatePath2D(int segments = 100) const;
    
    // Metadata
    void setDescription(const QString &desc) { m_description = desc; }
    QString description() const { return m_description; }
    
    QDateTime created() const { return m_created; }
    QDateTime modified() const { return m_modified; }
    
    // Access to keyframes
    const QVector<CameraKeyframe>& keyframes() const { return m_keyframes; }
    
private:
    void recalculateDuration();
    void updateModified();
    
    // Catmull-Rom interpolation
    float interpolateCatmullRom(float p0, float p1, float p2, float p3, float t) const;
    QPointF interpolateCatmullRom2D(const QPointF &p0, const QPointF &p1,
                                     const QPointF &p2, const QPointF &p3, float t) const;
    
    // Linear interpolation
    float lerp(float a, float b, float t) const;
    
    QVector<CameraKeyframe> m_keyframes;
    QString m_name;
    QString m_description;
    InterpolationType m_interpolation;
    bool m_loop;
    float m_totalDuration;
    QDateTime m_created;
    QDateTime m_modified;
};

#endif // CAMERAPATH_H
