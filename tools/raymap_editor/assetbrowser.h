#ifndef ASSETBROWSER_H
#define ASSETBROWSER_H

#include <QWidget>
#include <QTreeWidget>
#include <QFileSystemWatcher>

class AssetBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit AssetBrowser(QWidget *parent = nullptr);
    
    void setProjectPath(const QString &path);
    void refresh();

signals:
    void fileClicked(const QString &filePath);
    void fileDoubleClicked(const QString &filePath);
    void mapFileRequested(const QString &filePath);
    void fpgEditorRequested(const QString &filePath); // NEW: Request open in FPG editor

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onDirectoryChanged(const QString &path);
    void onContextMenu(const QPoint &pos);
    void onRenameFolder();
    void onCreateFolder();
    void onDeleteFolder();
    void onAddFile();
    void onNewCode();
    
private:
    void populateTree();
    void addDirectoryItems(QTreeWidgetItem *parent, const QString &path);
    void addDirectoryToWatcher(const QString &path);  // NEW: Recursive watcher
    QIcon getIconForFile(const QString &fileName);
    
    QTreeWidget *m_treeWidget;
    QPoint m_dragStartPos;
    QFileSystemWatcher *m_watcher;
    QString m_projectPath;
    QString m_selectedPath;  // For context menu operations
};

#endif // ASSETBROWSER_H
