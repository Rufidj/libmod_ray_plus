#include "objimportdialog.h"
#include "objtomd3converter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QProgressDialog>
#include <QApplication>

ObjImportDialog::ObjImportDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Conversor OBJ a MD3"));
    resize(400, 200);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Input
    QHBoxLayout *inputLayout = new QHBoxLayout();
    m_inputEdit = new QLineEdit();
    QPushButton *browseInputBtn = new QPushButton(tr("Buscar OBJ..."));
    inputLayout->addWidget(new QLabel(tr("Entrada:")));
    inputLayout->addWidget(m_inputEdit);
    inputLayout->addWidget(browseInputBtn);
    mainLayout->addLayout(inputLayout);
    
    // Output
    QHBoxLayout *outputLayout = new QHBoxLayout();
    m_outputEdit = new QLineEdit();
    QPushButton *browseOutputBtn = new QPushButton(tr("Salida MD3..."));
    outputLayout->addWidget(new QLabel(tr("Salida:")));
    outputLayout->addWidget(m_outputEdit);
    outputLayout->addWidget(browseOutputBtn);
    mainLayout->addLayout(outputLayout);
    
    // Options
    QHBoxLayout *optionsLayout = new QHBoxLayout();
    m_scaleSpin = new QDoubleSpinBox();
    m_scaleSpin->setRange(0.01, 1000.0);
    m_scaleSpin->setValue(1.0);
    m_scaleSpin->setSingleStep(0.1);
    
    m_atlasCheck = new QCheckBox(tr("Generar Atlas de Textura (PNG)"));
    
    m_atlasSizeSpin = new QSpinBox();
    m_atlasSizeSpin->setRange(64, 4096);
    m_atlasSizeSpin->setValue(1024);
    m_atlasSizeSpin->setSingleStep(128);
    m_atlasSizeSpin->setSuffix(" px");
    
    m_decimateSpin = new QSpinBox();
    m_decimateSpin->setRange(100, 20000);
    m_decimateSpin->setValue(2000); 
    m_decimateSpin->setSuffix(" tris");
    m_decimateSpin->setToolTip("Reducir triangulos si excede este numero");

    optionsLayout->addWidget(new QLabel(tr("Escala:")));
    optionsLayout->addWidget(m_scaleSpin);
    optionsLayout->addWidget(new QLabel(tr("Max Tris:")));
    optionsLayout->addWidget(m_decimateSpin);
    optionsLayout->addWidget(new QLabel(tr("Tam. Atlas:")));
    optionsLayout->addWidget(m_atlasSizeSpin);
    optionsLayout->addStretch();
    optionsLayout->addWidget(m_atlasCheck);
    mainLayout->addLayout(optionsLayout);
    
    // Buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    QPushButton *convertBtn = new QPushButton(tr("Convertir"));
    QPushButton *cancelBtn = new QPushButton(tr("Cerrar"));
    btnLayout->addStretch();
    btnLayout->addWidget(convertBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);
    
    // Connections
    connect(browseInputBtn, &QPushButton::clicked, this, &ObjImportDialog::browseInput);
    connect(browseOutputBtn, &QPushButton::clicked, this, &ObjImportDialog::browseOutput);
    connect(convertBtn, &QPushButton::clicked, this, &ObjImportDialog::convert);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void ObjImportDialog::browseInput()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Abrir Modelo"), "", tr("Modelos 3D (*.obj *.glb)"));
    if (!path.isEmpty()) {
        m_inputEdit->setText(path);
        // Auto-set output
        QFileInfo fi(path);
        QString out = fi.absolutePath() + "/" + fi.completeBaseName() + ".md3";
        m_outputEdit->setText(out);
    }
}

void ObjImportDialog::browseOutput()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Guardar MD3"), m_outputEdit->text(), tr("Quake 3 Model (*.md3)"));
    if (!path.isEmpty()) {
        m_outputEdit->setText(path);
    }
}

void ObjImportDialog::convert()
{
    QString inPath = m_inputEdit->text();
    QString outPath = m_outputEdit->text();
    
    if (inPath.isEmpty() || outPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Por favor selecciona archivos de entrada y salida correctly."));
        return;
    }
    
    QProgressDialog progress("Iniciando conversión...", "Cancelar", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);
    QApplication::processEvents();

    ObjToMd3Converter converter;
    converter.onProgress = [&](int p, QString s) {
        progress.setLabelText(s);
        progress.setValue(p);
        QApplication::processEvents();
        if (progress.wasCanceled()) {
             // Abort logic not fully implemented in converter yet
        }
    };
    
    bool success = false;
    
    if (inPath.endsWith(".glb", Qt::CaseInsensitive)) {
        success = converter.loadGlb(inPath);
    } else {
        success = converter.loadObj(inPath);
    }

    if (!success) {
        progress.close();
        QMessageBox::critical(this, tr("Error"), tr("No se pudo cargar el archivo de entrada.\nAsegúrate de que existe y es un formato válido."));
        return;
    }
    
    // Auto-decimate using UI Value
    int maxTris = m_decimateSpin->value();
    if (converter.triangleCount() > maxTris) {
        // Recalculate progress for decimate phase (60-80 range)
        progress.setLabelText("Reduciendo poligonos...");
        converter.decimate(maxTris);
    }
    
    // Prepare atlas path based on output name
    QFileInfo fi(outPath);
    QString atlasPath = fi.absolutePath() + "/" + fi.completeBaseName() + ".png";
    bool atlasCreated = false;

    // Merge textures if multiple materials found (GLB)
    if (converter.mergeTextures(atlasPath, m_atlasSizeSpin->value())) {
        atlasCreated = true;
    } 
    // Fallback to single texture baking if merge wasn't needed (1 material)
    else if (m_atlasCheck->isChecked()) {
        converter.setProgress(80, "Generando textura única...");
        if (converter.generateTextureAtlas(atlasPath, m_atlasSizeSpin->value())) {
            atlasCreated = true;
        }
    }
    
    converter.setProgress(90, "Guardando MD3...");
    if (!converter.saveMd3(outPath, m_scaleSpin->value())) {
        progress.close();
        QMessageBox::critical(this, tr("Error"), tr("No se pudo guardar el archivo MD3."));
        return;
    }
    
    progress.setValue(100);
    progress.close();
    
    QString msg = tr("Conversión completada con éxito!\n") + converter.debugInfo();
    if (atlasCreated) {
        msg += "\nAtlas texture: " + fi.completeBaseName() + ".png";
    }
}

QString ObjImportDialog::inputPath() const { return m_inputEdit->text(); }
QString ObjImportDialog::outputPath() const { return m_outputEdit->text(); }
double ObjImportDialog::scale() const { return m_scaleSpin->value(); }
bool ObjImportDialog::generateAtlas() const { return m_atlasCheck->isChecked(); }
