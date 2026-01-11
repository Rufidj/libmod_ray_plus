#include "camerapathcanvas.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>

CameraPathCanvas::CameraPathCanvas(QWidget *parent)
    : QWidget(parent)
    , m_path(nullptr)
    , m_selectedKeyframe(-1)
    , m_zoom(1.0f)
    , m_offset(0, 0)
    , m_draggingKeyframe(-1)
    , m_panning(false)
{
    setMinimumSize(400, 400);
    setMouseTracking(true);
}

void CameraPathCanvas::setMapData(const MapData &mapData)
{
    m_mapData = mapData;
    
    // Center view on map
    if (!m_mapData.sectors.isEmpty()) {
        // Calculate map bounds
        float minX = 1e9, minY = 1e9, maxX = -1e9, maxY = -1e9;
        for (const Sector &sector : m_mapData.sectors) {
            for (const Wall &wall : sector.walls) {
                minX = qMin(minX, qMin(wall.x1, wall.x2));
                minY = qMin(minY, qMin(wall.y1, wall.y2));
                maxX = qMax(maxX, qMax(wall.x1, wall.x2));
                maxY = qMax(maxY, qMax(wall.y1, wall.y2));
            }
        }
        
        float centerX = (minX + maxX) / 2.0f;
        float centerY = (minY + maxY) / 2.0f;
        m_offset = QPointF(-centerX, -centerY);
        
        // Adjust zoom to fit map
        float mapWidth = maxX - minX;
        float mapHeight = maxY - minY;
        float scaleX = width() / (mapWidth * 1.2f);
        float scaleY = height() / (mapHeight * 1.2f);
        m_zoom = qMin(scaleX, scaleY);
    }
    
    update();
}

void CameraPathCanvas::setCameraPath(CameraPath *path)
{
    m_path = path;
    update();
}

void CameraPathCanvas::setSelectedKeyframe(int index)
{
    m_selectedKeyframe = index;
    update();
}

void CameraPathCanvas::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Background
    painter.fillRect(rect(), QColor(40, 40, 40));
    
    // Transform
    painter.translate(width() / 2, height() / 2);
    painter.scale(m_zoom, m_zoom);
    painter.translate(m_offset);
    
    // Draw map
    drawMap(painter);
    
    // Draw path
    if (m_path) {
        drawPath(painter);
        drawKeyframes(painter);
    }
}

void CameraPathCanvas::drawMap(QPainter &painter)
{
    painter.setPen(QPen(QColor(100, 100, 100), 1.0f / m_zoom));
    
    for (const Sector &sector : m_mapData.sectors) {
        for (const Wall &wall : sector.walls) {
            painter.drawLine(QPointF(wall.x1, wall.y1),
                           QPointF(wall.x2, wall.y2));
        }
    }
}

void CameraPathCanvas::drawPath(QPainter &painter)
{
    if (m_path->keyframeCount() < 2) return;
    
    // Draw spline path
    QVector<QPointF> pathPoints = m_path->generatePath2D(100);
    
    painter.setPen(QPen(QColor(255, 255, 0, 180), 2.0f / m_zoom));
    for (int i = 0; i < pathPoints.size() - 1; i++) {
        painter.drawLine(pathPoints[i], pathPoints[i + 1]);
    }
    
    // Draw direction arrows
    painter.setPen(QPen(QColor(255, 200, 0), 1.5f / m_zoom));
    for (int i = 0; i < pathPoints.size() - 1; i += 10) {
        QPointF p1 = pathPoints[i];
        QPointF p2 = pathPoints[i + 1];
        QPointF dir = p2 - p1;
        float len = qSqrt(dir.x() * dir.x() + dir.y() * dir.y());
        if (len > 0.01f) {
            dir /= len;
            QPointF perp(-dir.y(), dir.x());
            
            float arrowSize = 5.0f / m_zoom;
            QPointF arrowTip = p1 + dir * arrowSize * 2;
            QPointF arrowLeft = p1 - dir * arrowSize + perp * arrowSize;
            QPointF arrowRight = p1 - dir * arrowSize - perp * arrowSize;
            
            painter.drawLine(arrowLeft, arrowTip);
            painter.drawLine(arrowRight, arrowTip);
        }
    }
}

void CameraPathCanvas::drawKeyframes(QPainter &painter)
{
    for (int i = 0; i < m_path->keyframeCount(); i++) {
        CameraKeyframe kf = m_path->getKeyframe(i);
        QPointF pos(kf.x, kf.y);
        
        float radius = 8.0f / m_zoom;
        
        // Color based on height (Z)
        int heightColor = qBound(0, (int)(kf.z * 2), 255);
        QColor kfColor;
        
        if (i == m_selectedKeyframe) {
            kfColor = QColor(255, 100, 100); // Selected = red
            painter.setPen(QPen(QColor(255, 255, 255), 2.0f / m_zoom));
        } else {
            // Blue to cyan based on height
            kfColor = QColor(100, 150, 100 + heightColor / 2);
            painter.setPen(QPen(QColor(255, 255, 255), 1.0f / m_zoom));
        }
        
        painter.setBrush(kfColor);
        painter.drawEllipse(pos, radius, radius);
        
        // Draw keyframe number
        painter.setPen(Qt::white);
        QFont font = painter.font();
        font.setPointSizeF(10.0f / m_zoom);
        painter.setFont(font);
        painter.drawText(QRectF(pos.x() - radius, pos.y() - radius,
                               radius * 2, radius * 2),
                        Qt::AlignCenter, QString::number(i + 1));
        
        // Draw height indicator (small line below)
        painter.setPen(QPen(QColor(200, 200, 200), 1.0f / m_zoom));
        float heightLine = (kf.z / 128.0f) * radius * 2;
        painter.drawLine(QPointF(pos.x(), pos.y() + radius + 2.0f / m_zoom),
                        QPointF(pos.x(), pos.y() + radius + 2.0f / m_zoom + heightLine));
    }
}

void CameraPathCanvas::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int kfIndex = findKeyframeAt(event->pos());
        
        if (kfIndex >= 0) {
            // Start dragging keyframe
            m_draggingKeyframe = kfIndex;
            emit keyframeSelected(kfIndex);
        } else {
            // Add new keyframe
            QPointF worldPos = screenToWorld(event->pos());
            emit keyframeAdded(worldPos.x(), worldPos.y());
        }
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void CameraPathCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_draggingKeyframe >= 0) {
        QPointF worldPos = screenToWorld(event->pos());
        emit keyframeMoved(m_draggingKeyframe, worldPos.x(), worldPos.y());
        update();
    } else if (m_panning) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_offset += QPointF(delta.x() / m_zoom, delta.y() / m_zoom);
        m_lastMousePos = event->pos();
        update();
    }
}

void CameraPathCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_draggingKeyframe = -1;
    } else if (event->button() == Qt::MiddleButton) {
        m_panning = false;
        setCursor(Qt::ArrowCursor);
    }
}

void CameraPathCanvas::wheelEvent(QWheelEvent *event)
{
    float zoomFactor = event->angleDelta().y() > 0 ? 1.1f : 0.9f;
    m_zoom *= zoomFactor;
    m_zoom = qBound(0.1f, m_zoom, 10.0f);
    update();
}

QPointF CameraPathCanvas::worldToScreen(float x, float y) const
{
    QPointF world(x, y);
    world += m_offset;
    world *= m_zoom;
    world += QPointF(width() / 2, height() / 2);
    return world;
}

QPointF CameraPathCanvas::screenToWorld(const QPoint &screen) const
{
    QPointF world = screen - QPointF(width() / 2, height() / 2);
    world /= m_zoom;
    world -= m_offset;
    return world;
}

int CameraPathCanvas::findKeyframeAt(const QPoint &pos) const
{
    if (!m_path) return -1;
    
    QPointF worldPos = screenToWorld(pos);
    float threshold = 15.0f / m_zoom;
    
    for (int i = 0; i < m_path->keyframeCount(); i++) {
        CameraKeyframe kf = m_path->getKeyframe(i);
        float dx = kf.x - worldPos.x();
        float dy = kf.y - worldPos.y();
        float dist = qSqrt(dx * dx + dy * dy);
        
        if (dist < threshold) {
            return i;
        }
    }
    
    return -1;
}
