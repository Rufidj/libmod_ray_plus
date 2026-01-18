#include "bennugdinstaller.h"
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QProcess>
#include <QDebug>

BennuGDInstaller::BennuGDInstaller(QWidget *parent)
    : QDialog(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
{
    setWindowTitle("Instalar BennuGD2");
    setMinimumWidth(500);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    m_statusLabel = new QLabel("Preparando instalación...", this);
    m_statusLabel->setWordWrap(true);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    
    m_cancelButton = new QPushButton("Cancelar", this);
    
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_cancelButton);
    
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

BennuGDInstaller::~BennuGDInstaller()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

void BennuGDInstaller::startInstallation()
{
    m_statusLabel->setText("Obteniendo última versión de BennuGD2...");
    m_progressBar->setValue(5);
    fetchLatestRelease();
}

void BennuGDInstaller::fetchLatestRelease()
{
    QUrl url("https://api.github.com/repos/Rufidj/BennuGD2/releases/latest");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "RayMapEditor");
    
    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &BennuGDInstaller::onLatestReleaseReceived);
}

void BennuGDInstaller::onLatestReleaseReceived()
{
    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = "No se pudo obtener la última versión de BennuGD2:\n" +
                          m_currentReply->errorString();
        qDebug() << "ERROR:" << errorMsg;
        QMessageBox::critical(this, "Error", errorMsg);
        reject();
        return;
    }
    
    QByteArray data = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    
    qDebug() << "Received release data:" << data.size() << "bytes";
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject release = doc.object();
    
    QString tagName = release["tag_name"].toString();
    QJsonArray assets = release["assets"].toArray();
    
    qDebug() << "Release tag:" << tagName;
    qDebug() << "Number of assets:" << assets.size();
    
    m_statusLabel->setText("Versión encontrada: " + tagName);
    m_progressBar->setValue(10);
    
    // Find asset for current OS
    QString assetName = getAssetNameForOS();
    qDebug() << "Looking for asset containing:" << assetName;
    
    QString downloadUrl;
    QString fallbackUrl;
    
    for (const QJsonValue &assetVal : assets) {
        QJsonObject asset = assetVal.toObject();
        QString name = asset["name"].toString();
        qDebug() << "  Checking asset:" << name;
        
        // Skip 32-bit versions explicitly
        if (name.contains("i386", Qt::CaseInsensitive) || 
            name.contains("i686", Qt::CaseInsensitive)) {
            qDebug() << "  Skipping 32-bit asset";
            continue;
        }
        
        if (name.contains(assetName, Qt::CaseInsensitive)) {
            downloadUrl = asset["browser_download_url"].toString();
            qDebug() << "  MATCH (64-bit)! Download URL:" << downloadUrl;
            break;
        }
    }
    
    
    if (downloadUrl.isEmpty()) {
        QString errorMsg = "No se encontró una versión compatible para tu sistema operativo.\n"
                          "Buscando: " + assetName + "\n"
                          "Por favor, descarga BennuGD2 manualmente desde:\n"
                          "https://github.com/Rufidj/BennuGD2/releases";
        qDebug() << "ERROR:" << errorMsg;
        QMessageBox::critical(this, "Error", errorMsg);
        reject();
        return;
    }
    
    downloadRelease(downloadUrl);
}

void BennuGDInstaller::downloadRelease(const QString &downloadUrl)
{
    qDebug() << "Starting download from:" << downloadUrl;
    
    m_statusLabel->setText("Descargando BennuGD2...");
    m_progressBar->setValue(15);
    
    QUrl url(downloadUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "RayMapEditor");
    
    // Follow redirects (GitHub uses redirects for release downloads)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    
    // Prepare temp file with correct extension
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString extension = ".tgz";
#ifdef Q_OS_WIN
    extension = ".rar";
#endif
    m_tempFilePath = tempDir + "/bennugd2_download" + extension;
    
    qDebug() << "Temp file path:" << m_tempFilePath;
    
    m_currentReply = m_networkManager->get(request);
    
    qDebug() << "Network request started";
    
    connect(m_currentReply, &QNetworkReply::downloadProgress,
            this, &BennuGDInstaller::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished,
            this, &BennuGDInstaller::onDownloadFinished);
}

void BennuGDInstaller::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percentage = 15 + (int)((bytesReceived * 70) / bytesTotal);
        m_progressBar->setValue(percentage);
        
        double mbReceived = bytesReceived / 1024.0 / 1024.0;
        double mbTotal = bytesTotal / 1024.0 / 1024.0;
        
        m_statusLabel->setText(QString("Descargando: %1 MB / %2 MB")
                              .arg(mbReceived, 0, 'f', 1)
                              .arg(mbTotal, 0, 'f', 1));
    }
}

void BennuGDInstaller::onDownloadFinished()
{
    qDebug() << "Download finished";
    qDebug() << "Error:" << m_currentReply->error();
    qDebug() << "Error string:" << m_currentReply->errorString();
    
    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = "Error al descargar BennuGD2:\n" + m_currentReply->errorString();
        qDebug() << "ERROR:" << errorMsg;
        QMessageBox::critical(this, "Error", errorMsg);
        reject();
        return;
    }
    
    QByteArray downloadedData = m_currentReply->readAll();
    qDebug() << "Downloaded" << downloadedData.size() << "bytes";
    
    // Save to temp file
    QFile file(m_tempFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QString errorMsg = "No se pudo guardar el archivo descargado: " + file.errorString();
        qDebug() << "ERROR:" << errorMsg;
        QMessageBox::critical(this, "Error", errorMsg);
        reject();
        return;
    }
    
    qint64 written = file.write(downloadedData);
    file.close();
    
    qDebug() << "Wrote" << written << "bytes to" << m_tempFilePath;
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    
    m_progressBar->setValue(85);
    
    // Extract and install
    extractAndInstall(m_tempFilePath);
}

void BennuGDInstaller::extractAndInstall(const QString &filePath)
{
    qDebug() << "Starting extraction from:" << filePath;
    
    m_statusLabel->setText("Extrayendo archivos...");
    m_progressBar->setValue(90);
    
    // Install directory
    QString installDir = QDir::homePath() + "/.bennugd2";
    QString binDir = installDir + "/bin";
    QDir().mkpath(binDir);
    
    qDebug() << "Install directory:" << installDir;
    qDebug() << "Bin directory:" << binDir;
    
    // Extract using tar (Linux/macOS) or 7z (Windows)
#ifdef Q_OS_WIN
    // TODO: Windows extraction
    QMessageBox::information(this, "Instalación manual requerida",
        "Por favor, extrae el archivo manualmente en:\n" + installDir);
    reject();
    return;
#else
    QProcess extractProcess;
    extractProcess.setWorkingDirectory(binDir);  // Extract directly to bin/
    
    QStringList args;
    args << "-xzf" << filePath;  // No --strip-components, files are at root
    
    qDebug() << "Running: tar" << args.join(" ");
    qDebug() << "Working directory:" << binDir;
    
    extractProcess.start("tar", args);
    bool finished = extractProcess.waitForFinished(30000);  // 30 seconds timeout
    
    qDebug() << "Process finished:" << finished;
    qDebug() << "Exit code:" << extractProcess.exitCode();
    qDebug() << "Exit status:" << extractProcess.exitStatus();
    
    QString stdOut = extractProcess.readAllStandardOutput();
    QString stdErr = extractProcess.readAllStandardError();
    
    if (!stdOut.isEmpty()) qDebug() << "STDOUT:" << stdOut;
    if (!stdErr.isEmpty()) qDebug() << "STDERR:" << stdErr;
    
    if (extractProcess.exitCode() != 0) {
        QString errorMsg = "Error al extraer archivos:\n" + stdErr;
        qDebug() << "ERROR:" << errorMsg;
        QMessageBox::critical(this, "Error", errorMsg);
        reject();
        return;
    }
#endif
    
    // Make binaries executable
    QString bgdcPath = installDir + "/bin/bgdc";
    QString bgdiPath = installDir + "/bin/bgdi";
    
    qDebug() << "Setting permissions for:" << bgdcPath;
    qDebug() << "Setting permissions for:" << bgdiPath;
    
    QFile::setPermissions(bgdcPath,
                         QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                         QFile::ReadGroup | QFile::ExeGroup |
                         QFile::ReadOther | QFile::ExeOther);
    
    QFile::setPermissions(bgdiPath,
                         QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                         QFile::ReadGroup | QFile::ExeGroup |
                         QFile::ReadOther | QFile::ExeOther);
    
    // Clean up
    QFile::remove(filePath);
    
    qDebug() << "Installation complete!";
    
    m_progressBar->setValue(100);
    m_statusLabel->setText("¡Instalación completada!");
    
    QMessageBox::information(this, "Éxito",
        "BennuGD2 se ha instalado correctamente en:\n" + installDir);
    
    emit installationFinished(true);
    accept();
}

QString BennuGDInstaller::getAssetNameForOS()
{
#ifdef Q_OS_LINUX
    // Linux: bgd2-linux-gnu-*.tgz or bgd2-i386-linux-gnu-*.tgz
    return "linux-gnu";  // Will match both x86_64 and i386
#elif defined(Q_OS_WIN)
    // Windows: bgd2-x86_64-w64-mingw32-*.rar or bgd2-i686-w64-mingw32-*.rar
    return "mingw32";  // Will match both x86_64 and i686
#elif defined(Q_OS_MAC)
    // macOS: x86_64-apple-darwin14-bgdc-*.app.tgz (separate files for bgdc and bgdi)
    return "apple-darwin";
#else
    return "unknown";
#endif
}
