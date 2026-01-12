
void RampGeneratorDialog::setTextures(const QMap<int, QPixmap> &textures)
{
    m_textureMap = textures;
}

void RampGeneratorDialog::onSelectFloorTexture()
{
    if (m_textureMap.isEmpty()) {
        QMessageBox::warning(this, tr("Sin texturas"),
                           tr("No hay texturas cargadas. Carga un FPG primero."));
        return;
    }
    
    TextureSelector *selector = new TextureSelector(m_textureMap, this);
    selector->setCurrentTexture(m_floorTextureSpin->value());
    
    if (selector->exec() == QDialog::Accepted) {
        int selectedId = selector->getSelectedTextureId();
        m_floorTextureSpin->setValue(selectedId);
    }
    
    delete selector;
}

void RampGeneratorDialog::onSelectCeilingTexture()
{
    if (m_textureMap.isEmpty()) {
        QMessageBox::warning(this, tr("Sin texturas"),
                           tr("No hay texturas cargadas. Carga un FPG primero."));
        return;
    }
    
    TextureSelector *selector = new TextureSelector(m_textureMap, this);
    selector->setCurrentTexture(m_ceilingTextureSpin->value());
    
    if (selector->exec() == QDialog::Accepted) {
        int selectedId = selector->getSelectedTextureId();
        m_ceilingTextureSpin->setValue(selectedId);
    }
    
    delete selector;
}

void RampGeneratorDialog::onSelectWallTexture()
{
    if (m_textureMap.isEmpty()) {
        QMessageBox::warning(this, tr("Sin texturas"),
                           tr("No hay texturas cargadas. Carga un FPG primero."));
        return;
    }
    
    TextureSelector *selector = new TextureSelector(m_textureMap, this);
    selector->setCurrentTexture(m_wallTextureSpin->value());
    
    if (selector->exec() == QDialog::Accepted) {
        int selectedId = selector->getSelectedTextureId();
        m_wallTextureSpin->setValue(selectedId);
    }
    
    delete selector;
}
