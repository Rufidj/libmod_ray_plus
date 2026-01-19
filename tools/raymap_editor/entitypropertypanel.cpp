#include "entitypropertypanel.h"
#include <QVBoxLayout>
#include <QGroupBox>

EntityPropertyPanel::EntityPropertyPanel(QWidget *parent)
    : QWidget(parent)
    , m_currentIndex(-1)
    , m_updating(false)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    QGroupBox *groupBox = new QGroupBox(); // Removed title to avoid duplication with Dock title
    QFormLayout *layout = new QFormLayout(groupBox);
    
    // Type / Name
    m_nameEdit = new QLineEdit();
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &EntityPropertyPanel::onTypeChanged);
    layout->addRow(tr("Nombre Proceso:"), m_nameEdit);
    
    m_typeEdit = new QLineEdit();
    m_typeEdit->setReadOnly(true); // Usually derived from asset
    layout->addRow(tr("Tipo:"), m_typeEdit);
    
    m_assetEdit = new QLineEdit();
    m_assetEdit->setReadOnly(true);
    layout->addRow(tr("Asset:"), m_assetEdit);
    
    // Coordinates
    m_xSpin = new QDoubleSpinBox();
    setupSpinBox(m_xSpin, " u");
    layout->addRow(tr("Posición X:"), m_xSpin);
    
    m_ySpin = new QDoubleSpinBox();
    setupSpinBox(m_ySpin, " u");
    layout->addRow(tr("Posición Y (Depth):"), m_ySpin);
    
    m_zSpin = new QDoubleSpinBox();
    setupSpinBox(m_zSpin, " u");
    layout->addRow(tr("Altura Z:"), m_zSpin);
    
    mainLayout->addWidget(groupBox);
    mainLayout->addStretch();
    
    // Start disabled
    clearSelection();
}

void EntityPropertyPanel::setupSpinBox(QDoubleSpinBox *spin, const QString &suffix)
{
    spin->setRange(-100000.0, 100000.0);
    spin->setSingleStep(1.0);
    spin->setDecimals(1);
    spin->setSuffix(suffix);
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &EntityPropertyPanel::onValueChanged);
}

void EntityPropertyPanel::setEntity(int index, const EntityInstance &entity)
{
    m_updating = true;
    m_currentIndex = index;
    m_currentEntity = entity;
    
    m_nameEdit->setText(entity.processName);
    m_typeEdit->setText(entity.type);
    m_assetEdit->setText(entity.assetPath);
    
    m_xSpin->setValue(entity.x);
    m_ySpin->setValue(entity.y);
    m_zSpin->setValue(entity.z);
    
    setEnabled(true);
    m_updating = false;
}

void EntityPropertyPanel::clearSelection()
{
    m_updating = true;
    m_currentIndex = -1;
    
    m_nameEdit->clear();
    m_typeEdit->clear();
    m_assetEdit->clear();
    m_xSpin->setValue(0);
    m_ySpin->setValue(0);
    m_zSpin->setValue(0);
    
    setEnabled(false);
    m_updating = false;
}

void EntityPropertyPanel::onValueChanged()
{
    if (m_updating || m_currentIndex < 0) return;
    
    m_currentEntity.x = m_xSpin->value();
    m_currentEntity.y = m_ySpin->value();
    m_currentEntity.z = m_zSpin->value();
    
    emit entityChanged(m_currentIndex, m_currentEntity);
}

void EntityPropertyPanel::onTypeChanged()
{
    if (m_updating || m_currentIndex < 0) return;
    
    m_currentEntity.processName = m_nameEdit->text();
    // Type and Asset Path usually shouldn't be changed manually here without updating the asset,
    // but processName is fine.
    
    emit entityChanged(m_currentIndex, m_currentEntity);
}
