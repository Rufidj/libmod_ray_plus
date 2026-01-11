#ifndef CAMERAKEYFRAME_H
#define CAMERAKEYFRAME_H

#include <QString>

enum class EaseType {
    LINEAR,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT,
    EASE_IN_CUBIC,
    EASE_OUT_CUBIC,
    EASE_IN_OUT_CUBIC
};

struct CameraKeyframe {
    // Position in 3D space
    float x, y, z;
    
    // Rotation (degrees)
    float yaw;      // Horizontal rotation
    float pitch;    // Vertical tilt
    float roll;     // Camera roll
    
    // Camera properties
    float fov;      // Field of view (degrees)
    
    // Timing
    float time;     // Time in seconds from start of sequence
    float duration; // How long to stay at this point (pause)
    
    // Animation curves
    EaseType easeIn;
    EaseType easeOut;
    
    // Speed multiplier for segment leading to this keyframe
    float speedMultiplier;
    
    // Constructor with defaults
    CameraKeyframe()
        : x(0), y(0), z(64)
        , yaw(0), pitch(0), roll(0)
        , fov(90)
        , time(0), duration(0)
        , easeIn(EaseType::LINEAR)
        , easeOut(EaseType::LINEAR)
        , speedMultiplier(1.0f)
    {}
    
    CameraKeyframe(float px, float py, float pz)
        : x(px), y(py), z(pz)
        , yaw(0), pitch(0), roll(0)
        , fov(90)
        , time(0), duration(0)
        , easeIn(EaseType::LINEAR)
        , easeOut(EaseType::LINEAR)
        , speedMultiplier(1.0f)
    {}
};

// Easing functions
inline float easeLinear(float t) {
    return t;
}

inline float easeIn(float t) {
    return t * t;
}

inline float easeOut(float t) {
    return t * (2.0f - t);
}

inline float easeInOut(float t) {
    return t < 0.5f 
        ? 2.0f * t * t 
        : -1.0f + (4.0f - 2.0f * t) * t;
}

inline float easeInCubic(float t) {
    return t * t * t;
}

inline float easeOutCubic(float t) {
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}

inline float easeInOutCubic(float t) {
    return t < 0.5f
        ? 4.0f * t * t * t
        : 1.0f + (--t) * (2.0f * (--t)) * (2.0f * t);
}

inline float applyEase(float t, EaseType type) {
    switch (type) {
        case EaseType::LINEAR: return easeLinear(t);
        case EaseType::EASE_IN: return easeIn(t);
        case EaseType::EASE_OUT: return easeOut(t);
        case EaseType::EASE_IN_OUT: return easeInOut(t);
        case EaseType::EASE_IN_CUBIC: return easeInCubic(t);
        case EaseType::EASE_OUT_CUBIC: return easeOutCubic(t);
        case EaseType::EASE_IN_OUT_CUBIC: return easeInOutCubic(t);
        default: return t;
    }
}

inline QString easeTypeToString(EaseType type) {
    switch (type) {
        case EaseType::LINEAR: return "Linear";
        case EaseType::EASE_IN: return "Ease In";
        case EaseType::EASE_OUT: return "Ease Out";
        case EaseType::EASE_IN_OUT: return "Ease In/Out";
        case EaseType::EASE_IN_CUBIC: return "Ease In Cubic";
        case EaseType::EASE_OUT_CUBIC: return "Ease Out Cubic";
        case EaseType::EASE_IN_OUT_CUBIC: return "Ease In/Out Cubic";
        default: return "Linear";
    }
}

#endif // CAMERAKEYFRAME_H
