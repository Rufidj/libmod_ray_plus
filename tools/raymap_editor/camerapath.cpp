#include "camerapath.h"

CameraPath::CameraPath()
    : m_interpolation(InterpolationType::CATMULL_ROM)
    , m_loop(false)
    , m_totalDuration(0.0f)
    , m_created(QDateTime::currentDateTime())
    , m_modified(QDateTime::currentDateTime())
{
}

void CameraPath::addKeyframe(const CameraKeyframe &kf)
{
    m_keyframes.append(kf);
    recalculateDuration();
    updateModified();
}

void CameraPath::removeKeyframe(int index)
{
    if (index >= 0 && index < m_keyframes.size()) {
        m_keyframes.removeAt(index);
        recalculateDuration();
        updateModified();
    }
}

void CameraPath::updateKeyframe(int index, const CameraKeyframe &kf)
{
    if (index >= 0 && index < m_keyframes.size()) {
        m_keyframes[index] = kf;
        recalculateDuration();
        updateModified();
    }
}

CameraKeyframe CameraPath::getKeyframe(int index) const
{
    if (index >= 0 && index < m_keyframes.size()) {
        return m_keyframes[index];
    }
    return CameraKeyframe();
}

void CameraPath::recalculateDuration()
{
    if (m_keyframes.isEmpty()) {
        m_totalDuration = 0.0f;
        return;
    }
    
    // Sort keyframes by time
    std::sort(m_keyframes.begin(), m_keyframes.end(),
              [](const CameraKeyframe &a, const CameraKeyframe &b) {
                  return a.time < b.time;
              });
    
    // Total duration is the time of the last keyframe plus its duration
    m_totalDuration = m_keyframes.last().time + m_keyframes.last().duration;
}

void CameraPath::updateModified()
{
    m_modified = QDateTime::currentDateTime();
}

CameraKeyframe CameraPath::interpolateAt(float time) const
{
    if (m_keyframes.isEmpty()) {
        return CameraKeyframe();
    }
    
    if (m_keyframes.size() == 1) {
        return m_keyframes[0];
    }
    
    // Handle loop
    if (m_loop && time > m_totalDuration) {
        time = fmod(time, m_totalDuration);
    }
    
    // Clamp time
    if (time <= m_keyframes.first().time) {
        return m_keyframes.first();
    }
    if (time >= m_keyframes.last().time) {
        return m_keyframes.last();
    }
    
    // Find surrounding keyframes
    int i1 = 0, i2 = 0;
    for (int i = 0; i < m_keyframes.size() - 1; i++) {
        if (time >= m_keyframes[i].time && time <= m_keyframes[i + 1].time) {
            i1 = i;
            i2 = i + 1;
            break;
        }
    }
    
    const CameraKeyframe &kf1 = m_keyframes[i1];
    const CameraKeyframe &kf2 = m_keyframes[i2];
    
    // Calculate interpolation factor
    float segmentDuration = kf2.time - kf1.time;
    float t = (time - kf1.time) / segmentDuration;
    
    // Apply easing
    t = applyEase(t, kf1.easeOut);
    
    // Apply speed multiplier
    t *= kf2.speedMultiplier;
    t = qBound(0.0f, t, 1.0f);
    
    CameraKeyframe result;
    
    if (m_interpolation == InterpolationType::LINEAR) {
        // Linear interpolation
        result.x = lerp(kf1.x, kf2.x, t);
        result.y = lerp(kf1.y, kf2.y, t);
        result.z = lerp(kf1.z, kf2.z, t);
        result.yaw = lerp(kf1.yaw, kf2.yaw, t);
        result.pitch = lerp(kf1.pitch, kf2.pitch, t);
        result.roll = lerp(kf1.roll, kf2.roll, t);
        result.fov = lerp(kf1.fov, kf2.fov, t);
    } else if (m_interpolation == InterpolationType::CATMULL_ROM) {
        // Catmull-Rom spline interpolation
        // Get 4 control points (p0, p1, p2, p3)
        int i0 = qMax(0, i1 - 1);
        int i3 = qMin(m_keyframes.size() - 1, i2 + 1);
        
        const CameraKeyframe &kf0 = m_keyframes[i0];
        const CameraKeyframe &kf3 = m_keyframes[i3];
        
        result.x = interpolateCatmullRom(kf0.x, kf1.x, kf2.x, kf3.x, t);
        result.y = interpolateCatmullRom(kf0.y, kf1.y, kf2.y, kf3.y, t);
        result.z = interpolateCatmullRom(kf0.z, kf1.z, kf2.z, kf3.z, t);
        result.yaw = interpolateCatmullRom(kf0.yaw, kf1.yaw, kf2.yaw, kf3.yaw, t);
        result.pitch = interpolateCatmullRom(kf0.pitch, kf1.pitch, kf2.pitch, kf3.pitch, t);
        result.roll = interpolateCatmullRom(kf0.roll, kf1.roll, kf2.roll, kf3.roll, t);
        result.fov = interpolateCatmullRom(kf0.fov, kf1.fov, kf2.fov, kf3.fov, t);
    }
    
    result.time = time;
    return result;
}

QVector<QPointF> CameraPath::generatePath2D(int segments) const
{
    QVector<QPointF> path;
    
    if (m_keyframes.size() < 2) {
        if (!m_keyframes.isEmpty()) {
            path.append(QPointF(m_keyframes[0].x, m_keyframes[0].y));
        }
        return path;
    }
    
    float step = m_totalDuration / segments;
    for (int i = 0; i <= segments; i++) {
        float time = i * step;
        CameraKeyframe kf = interpolateAt(time);
        path.append(QPointF(kf.x, kf.y));
    }
    
    return path;
}

float CameraPath::interpolateCatmullRom(float p0, float p1, float p2, float p3, float t) const
{
    float t2 = t * t;
    float t3 = t2 * t;
    
    return 0.5f * (
        (2.0f * p1) +
        (-p0 + p2) * t +
        (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
        (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
    );
}

QPointF CameraPath::interpolateCatmullRom2D(const QPointF &p0, const QPointF &p1,
                                             const QPointF &p2, const QPointF &p3, float t) const
{
    float x = interpolateCatmullRom(p0.x(), p1.x(), p2.x(), p3.x(), t);
    float y = interpolateCatmullRom(p0.y(), p1.y(), p2.y(), p3.y(), t);
    return QPointF(x, y);
}

float CameraPath::lerp(float a, float b, float t) const
{
    return a + (b - a) * t;
}
