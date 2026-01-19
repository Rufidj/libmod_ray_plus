#ifndef ENTITYPROPERTYPANEL_H
#define ENTITYPROPERTYPANEL_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QFormLayout>
#include "mapdata.h"

class EntityPropertyPanel : public QWidget
{
    Q_OBJECT
    
public:
    explicit EntityPropertyPanel(QWidget *parent = nullptr);
    
    void setEntity(int index, const EntityInstance &entity);
    void clearSelection();
    
signals:
    void entityChanged(int index, const EntityInstance &entity);
    
private slots:
    void onValueChanged();
    void onTypeChanged();
    
private:
    int m_currentIndex;
    EntityInstance m_currentEntity;
    bool m_updating;
    
    QLineEdit *m_typeEdit;
    QLineEdit *m_nameEdit; // processName
    QLineEdit *m_assetEdit;
    
    QDoubleSpinBox *m_xSpin;
    QDoubleSpinBox *m_ySpin; // Depth (RayMap Z / GL Z)
    QDoubleSpinBox *m_zSpin; // Height (RayMap Y / GL Y) - wait, check coordinate system
    
    // In RayMap data:
    // x, y (2D map coordinates), z (height)
    // Actually in RayMapFormat:
    // x = x, y = z (height), z = y (depth) ?
    // Let's stick to the struct fields: x, y, z.
    
    // Helper to setup spins
    void setupSpinBox(QDoubleSpinBox *spin, const QString &suffix);
};

#endif // ENTITYPROPERTYPANEL_H
