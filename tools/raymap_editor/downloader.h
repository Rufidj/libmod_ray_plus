#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>

class Downloader : public QObject
{
    Q_OBJECT
public:
    explicit Downloader(QObject *parent = nullptr);
    void download(const QString &url, const QString &destPath);

signals:
    void progress(qint64 received, qint64 total);
    void finished(bool success, QString message);

private slots:
    void onDownloadProgress(qint64 bytesRead, qint64 totalBytes);
    void onFinished();
    void onReadyRead(); // NEW

private:
    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    QFile *m_file;
    QString m_destPath;
};

class DownloadDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DownloadDialog(const QString &url, const QString &destPath, const QString &title, bool autoUnzip = false, QWidget *parent = nullptr);
    bool start();

private:
    Downloader *m_downloader;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QString m_url;
    QString m_destPath;
    bool m_autoUnzip;
};

#endif // DOWNLOADER_H
