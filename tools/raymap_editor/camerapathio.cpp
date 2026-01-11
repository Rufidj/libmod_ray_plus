#include "camerapathio.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

bool CameraPathIO::save(const CameraPath &path, const QString &filename)
{
    QJsonObject root;
    root["version"] = "1.0";
    root["name"] = path.name();
    root["description"] = path.description();
    root["loop"] = path.loop();
    root["interpolation"] = (int)path.interpolation();
    root["totalDuration"] = path.totalDuration();
    root["created"] = path.created().toString(Qt::ISODate);
    root["modified"] = path.modified().toString(Qt::ISODate);
    
    QJsonArray keyframesArray;
    for (const CameraKeyframe &kf : path.keyframes()) {
        keyframesArray.append(keyframeToJson(kf));
    }
    root["keyframes"] = keyframesArray;
    
    QJsonDocument doc(root);
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

CameraPath CameraPathIO::load(const QString &filename, bool *ok)
{
    CameraPath path;
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return path;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        if (ok) *ok = false;
        return path;
    }
    
    QJsonObject root = doc.object();
    
    path.setName(root["name"].toString());
    path.setDescription(root["description"].toString());
    path.setLoop(root["loop"].toBool());
    path.setInterpolation((InterpolationType)root["interpolation"].toInt());
    
    QJsonArray keyframesArray = root["keyframes"].toArray();
    for (const QJsonValue &val : keyframesArray) {
        if (val.isObject()) {
            path.addKeyframe(jsonToKeyframe(val.toObject()));
        }
    }
    
    if (ok) *ok = true;
    return path;
}

QJsonObject CameraPathIO::keyframeToJson(const CameraKeyframe &kf)
{
    QJsonObject obj;
    
    QJsonObject pos;
    pos["x"] = kf.x;
    pos["y"] = kf.y;
    pos["z"] = kf.z;
    obj["position"] = pos;
    
    QJsonObject rot;
    rot["yaw"] = kf.yaw;
    rot["pitch"] = kf.pitch;
    rot["roll"] = kf.roll;
    obj["rotation"] = rot;
    
    obj["fov"] = kf.fov;
    obj["time"] = kf.time;
    obj["duration"] = kf.duration;
    obj["easeIn"] = easeTypeToString(kf.easeIn);
    obj["easeOut"] = easeTypeToString(kf.easeOut);
    obj["speedMultiplier"] = kf.speedMultiplier;
    
    return obj;
}

CameraKeyframe CameraPathIO::jsonToKeyframe(const QJsonObject &obj)
{
    CameraKeyframe kf;
    
    QJsonObject pos = obj["position"].toObject();
    kf.x = pos["x"].toDouble();
    kf.y = pos["y"].toDouble();
    kf.z = pos["z"].toDouble();
    
    QJsonObject rot = obj["rotation"].toObject();
    kf.yaw = rot["yaw"].toDouble();
    kf.pitch = rot["pitch"].toDouble();
    kf.roll = rot["roll"].toDouble();
    
    kf.fov = obj["fov"].toDouble(90.0);
    kf.time = obj["time"].toDouble();
    kf.duration = obj["duration"].toDouble();
    kf.easeIn = stringToEaseType(obj["easeIn"].toString());
    kf.easeOut = stringToEaseType(obj["easeOut"].toString());
    kf.speedMultiplier = obj["speedMultiplier"].toDouble(1.0);
    
    return kf;
}

QString CameraPathIO::easeTypeToString(EaseType type)
{
    switch (type) {
        case EaseType::LINEAR: return "linear";
        case EaseType::EASE_IN: return "ease_in";
        case EaseType::EASE_OUT: return "ease_out";
        case EaseType::EASE_IN_OUT: return "ease_in_out";
        case EaseType::EASE_IN_CUBIC: return "ease_in_cubic";
        case EaseType::EASE_OUT_CUBIC: return "ease_out_cubic";
        case EaseType::EASE_IN_OUT_CUBIC: return "ease_in_out_cubic";
        default: return "linear";
    }
}

EaseType CameraPathIO::stringToEaseType(const QString &str)
{
    if (str == "ease_in") return EaseType::EASE_IN;
    if (str == "ease_out") return EaseType::EASE_OUT;
    if (str == "ease_in_out") return EaseType::EASE_IN_OUT;
    if (str == "ease_in_cubic") return EaseType::EASE_IN_CUBIC;
    if (str == "ease_out_cubic") return EaseType::EASE_OUT_CUBIC;
    if (str == "ease_in_out_cubic") return EaseType::EASE_IN_OUT_CUBIC;
    return EaseType::LINEAR;
}
