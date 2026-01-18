#ifndef BENNUGDINSTALLER_H
#define BENNUGDINSTALLER_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class BennuGDInstaller : public QDialog
{
    Q_OBJECT

public:
    explicit BennuGDInstaller(QWidget *parent = nullptr);
    ~BennuGDInstaller();
    
    void startInstallation();

signals:
    void installationFinished(bool success);

private slots:
    void onLatestReleaseReceived();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    void fetchLatestRelease();
    void downloadRelease(const QString &downloadUrl);
    void extractAndInstall(const QString &filePath);
    QString getAssetNameForOS();
    
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_cancelButton;
    
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    
    QString m_downloadUrl;
    QString m_tempFilePath;
};

#endif // BENNUGDINSTALLER_H
