#ifndef OBJIMPORTDIALOG_H
#define OBJIMPORTDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>

class ObjImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ObjImportDialog(QWidget *parent = nullptr);
    
    QString inputPath() const;
    QString outputPath() const;
    double scale() const;
    bool generateAtlas() const;

private slots:
    void browseInput();
    void browseOutput();
    void convert();

private:
    QLineEdit *m_inputEdit;
    QLineEdit *m_outputEdit;
    QDoubleSpinBox *m_scaleSpin;
    QCheckBox *m_atlasCheck;
    QSpinBox *m_atlasSizeSpin;
    QSpinBox *m_decimateSpin;
};

#endif // OBJIMPORTDIALOG_H
