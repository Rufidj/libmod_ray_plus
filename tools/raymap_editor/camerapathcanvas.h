#ifndef CAMERAPATHCANVAS_H
#define CAMERAPATHCANVAS_H

#include <QWidget>
#include <QPixmap>
#include "camerapath.h"
#include "mapdata.h"

class CameraPathCanvas : public QWidget
{
    Q_OBJECT

public:
    explicit CameraPathCanvas(QWidget *parent = nullptr);
    
    void setMapData(const MapData &mapData);
    void setCameraPath(CameraPath *path);
    void setSelectedKeyframe(int index);
    
signals:
    void keyframeAdded(float x, float y);
    void keyframeSelected(int index);
    void keyframeMoved(int index, float x, float y);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void drawMap(QPainter &painter);
    void drawPath(QPainter &painter);
    void drawKeyframes(QPainter &painter);
    
    QPointF worldToScreen(float x, float y) const;
    QPointF screenToWorld(const QPoint &screen) const;
    int findKeyframeAt(const QPoint &pos) const;
    
    MapData m_mapData;
    CameraPath *m_path;
    int m_selectedKeyframe;
    
    // View transform
    float m_zoom;
    QPointF m_offset;
    
    // Interaction
    int m_draggingKeyframe;
    QPoint m_lastMousePos;
    bool m_panning;
};

#endif // CAMERAPATHCANVAS_H
