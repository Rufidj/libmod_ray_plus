#ifndef CAMERAPATHIO_H
#define CAMERAPATHIO_H

#include "camerapath.h"
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

class CameraPathIO
{
public:
    static bool save(const CameraPath &path, const QString &filename);
    static CameraPath load(const QString &filename, bool *ok = nullptr);
    
private:
    static QJsonObject keyframeToJson(const CameraKeyframe &kf);
    static CameraKeyframe jsonToKeyframe(const QJsonObject &obj);
    static QString easeTypeToString(EaseType type);
    static EaseType stringToEaseType(const QString &str);
};

#endif // CAMERAPATHIO_H
