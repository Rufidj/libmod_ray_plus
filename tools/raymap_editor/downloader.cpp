#include "downloader.h"
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QProcess>

Downloader::Downloader(QObject *parent) : QObject(parent), m_manager(new QNetworkAccessManager(this)), m_reply(nullptr), m_file(nullptr)
{
}

void Downloader::download(const QString &url, const QString &destPath)
{
    m_destPath = destPath;
    
    // Ensure directory exists
    QFileInfo fi(destPath);
    QDir().mkpath(fi.absolutePath());
    
    m_file = new QFile(destPath);
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit finished(false, "No se pudo crear el archivo de destino: " + destPath);
        delete m_file;
        m_file = nullptr;
        return;
    }
    
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // User-Agent often helps with direct downloads from google
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Qt; BennetGD2)"); 
    m_reply = m_manager->get(request);
    
    connect(m_reply, &QNetworkReply::downloadProgress, this, &Downloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::readyRead, this, &Downloader::onReadyRead); // NEW
    connect(m_reply, &QNetworkReply::finished, this, &Downloader::onFinished);
}

void Downloader::onReadyRead()
{
    if (m_file && m_file->isOpen()) {
        m_file->write(m_reply->readAll());
    }
}

void Downloader::onDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    emit progress(bytesRead, totalBytes);
}

void Downloader::onFinished()
{
    m_file->close();
    
    if (m_reply->error()) {
        m_file->remove();
        emit finished(false, "Error de descarga: " + m_reply->errorString());
    } else {
        // Set executable permissions if needed (simple heuristic: no extension or .AppImage)
        if (!m_destPath.contains(".") || m_destPath.endsWith(".AppImage")) {
             m_file->setPermissions(QFile::ExeUser | QFile::ExeGroup | QFile::ExeOther | QFile::ReadOwner | QFile::ReadGroup | QFile::ReadOther);
        }
        emit finished(true, "Descarga completada.");
    }
    
    m_reply->deleteLater();
    m_reply = nullptr;
    delete m_file;
    m_file = nullptr;
}

// Dialog Implementation
DownloadDialog::DownloadDialog(const QString &url, const QString &destPath, const QString &title, bool autoUnzip, QWidget *parent)
    : QDialog(parent), m_url(url), m_destPath(destPath), m_autoUnzip(autoUnzip)
{
    setWindowTitle(title);
    resize(400, 150);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    m_statusLabel = new QLabel(tr("Iniciando descarga..."), this);
    layout->addWidget(m_statusLabel);
    
    m_progressBar = new QProgressBar(this);
    layout->addWidget(m_progressBar);
    
    QPushButton *cancelBtn = new QPushButton(tr("Cancelar"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    layout->addWidget(cancelBtn);
    
    m_downloader = new Downloader(this);
    connect(m_downloader, &Downloader::progress, [this](qint64 received, qint64 total){
        if (total > 0) {
            m_progressBar->setMaximum(100);
            m_progressBar->setValue((received * 100) / total);
            m_statusLabel->setText(tr("Descargando... %1 MB / %2 MB")
                                   .arg(received / 1024.0 / 1024.0, 0, 'f', 1)
                                   .arg(total / 1024.0 / 1024.0, 0, 'f', 1));
        } else {
            m_progressBar->setMaximum(0); 
        }
    });
    
    connect(m_downloader, &Downloader::finished, [this](bool success, QString msg){
        if (success) {
            if (m_autoUnzip) {
                m_statusLabel->setText(tr("Descomprimiendo..."));
                m_progressBar->setMaximum(0);
                QCoreApplication::processEvents();
                
                QProcess unzip;
                unzip.setWorkingDirectory(QFileInfo(m_destPath).path());
                unzip.start("unzip", QStringList() << "-o" << m_destPath); // -o overwrite
                if (unzip.waitForFinished() && unzip.exitCode() == 0) {
                     QFile::remove(m_destPath); // Remove zip after extraction
                     accept();
                } else {
                     QMessageBox::critical(this, tr("Error"), tr("Error al descomprimir: %1").arg(QString(unzip.readAllStandardError())));
                     reject();
                }
            } else {
                accept();
            }
        } else {
            QMessageBox::critical(this, tr("Error"), msg);
            reject();
        }
    });
}

bool DownloadDialog::start()
{
    m_downloader->download(m_url, m_destPath);
    return exec() == QDialog::Accepted;
}
