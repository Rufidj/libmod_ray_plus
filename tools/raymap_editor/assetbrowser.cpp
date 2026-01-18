#include "assetbrowser.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QDebug>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QMouseEvent>
#include <QFile>
#include <QTextStream>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
#include <QApplication>

AssetBrowser::AssetBrowser(QWidget *parent)
    : QWidget(parent)
    , m_watcher(new QFileSystemWatcher(this))
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    /* Toolbar removed to fix UI overlap issues
    // Toolbar with refresh button
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    QPushButton *refreshBtn = new QPushButton(QIcon::fromTheme("view-refresh"), "");
    refreshBtn->setToolTip("Actualizar");
    refreshBtn->setMaximumWidth(30);
    connect(refreshBtn, &QPushButton::clicked, this, &AssetBrowser::refresh);
    toolbarLayout->addWidget(refreshBtn);
    toolbarLayout->addStretch();
    layout->addLayout(toolbarLayout);
    */
    
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabel("Assets del Proyecto");
    m_treeWidget->setColumnCount(1);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setDragEnabled(true);
    m_treeWidget->setDragDropMode(QAbstractItemView::DragOnly);
    layout->addWidget(m_treeWidget);
    
    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, &AssetBrowser::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &AssetBrowser::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &AssetBrowser::onContextMenu);
    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &AssetBrowser::onItemClicked);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &AssetBrowser::onDirectoryChanged);
    
    // Enable drag and drop - use startDrag override instead of itemPressed
    m_treeWidget->viewport()->installEventFilter(this);
}

bool AssetBrowser::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_treeWidget->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_dragStartPos = mouseEvent->pos();
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                int distance = (mouseEvent->pos() - m_dragStartPos).manhattanLength();
                if (distance >= QApplication::startDragDistance()) {
                    QTreeWidgetItem *item = m_treeWidget->itemAt(m_dragStartPos);
                    if (item) {
                        QString filePath = item->data(0, Qt::UserRole).toString();
                        QFileInfo info(filePath);
                        
                        if (info.isFile()) {
                            QDrag *drag = new QDrag(this);
                            QMimeData *mimeData = new QMimeData();
                            
                            QList<QUrl> urls;
                            urls << QUrl::fromLocalFile(filePath);
                            mimeData->setUrls(urls);
                            
                            drag->setMimeData(mimeData);
                            drag->exec(Qt::CopyAction);
                        }
                    }
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

void AssetBrowser::setProjectPath(const QString &path)
{
    m_projectPath = path;
    
    // Clear old watches
    if (!m_watcher->directories().isEmpty()) {
        m_watcher->removePaths(m_watcher->directories());
    }
    
    // Watch project path and all subdirectories recursively
    if (!path.isEmpty()) {
        QDir projectDir(path);
        if (projectDir.exists()) {
            m_watcher->addPath(path);
            addDirectoryToWatcher(path);
        }
    }
    
    refresh();
}

void AssetBrowser::refresh()
{
    m_treeWidget->clear();
    
    if (m_projectPath.isEmpty()) {
        return;
    }
    
    populateTree();
}

void AssetBrowser::populateTree()
{
    QDir projectDir(m_projectPath);
    if (!projectDir.exists()) {
        return;
    }
    
    // Add main folders
    QStringList folders = {"src", "assets", "build"};
    
    for (const QString &folder : folders) {
        QString folderPath = m_projectPath + "/" + folder;
        QDir dir(folderPath);
        
        if (dir.exists()) {
            QTreeWidgetItem *folderItem = new QTreeWidgetItem(m_treeWidget);
            folderItem->setText(0, folder);
            folderItem->setIcon(0, QIcon::fromTheme("folder"));
            folderItem->setData(0, Qt::UserRole, folderPath);
            
            addDirectoryItems(folderItem, folderPath);
            folderItem->setExpanded(true);
        }
    }
}

void AssetBrowser::addDirectoryItems(QTreeWidgetItem *parent, const QString &path)
{
    QDir dir(path);
    
    // Add subdirectories first
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &dirInfo : dirs) {
        QTreeWidgetItem *dirItem = new QTreeWidgetItem(parent);
        dirItem->setText(0, dirInfo.fileName());
        dirItem->setIcon(0, QIcon::fromTheme("folder"));
        dirItem->setData(0, Qt::UserRole, dirInfo.absoluteFilePath());
        
        addDirectoryItems(dirItem, dirInfo.absoluteFilePath());
    }
    
    // Add files with filtering based on folder name
    QFileInfoList files = dir.entryInfoList(QDir::Files, QDir::Name);
    
    // Determine file filter based on directory name
    QString dirName = QFileInfo(path).fileName().toLower();
    QStringList allowedExtensions;
    
    if (dirName == "models" || dirName == "model") {
        allowedExtensions << "md3" << "obj" << "fbx" << "png" << "jpg";
    } else if (dirName == "textures" || dirName == "texture") {
        allowedExtensions << "png" << "jpg" << "bmp" << "tga" << "fpg" << "map";
    } else if (dirName == "sprites" || dirName == "sprite" || dirName == "fpg") {
        allowedExtensions << "png" << "jpg" << "bmp" << "fpg" << "map";
    } else if (dirName == "src" || dirName == "includes") {
        allowedExtensions << "prg" << "h" << "c";
    } else {
        // No filter for other directories - show all files
        allowedExtensions.clear();
    }
    
    for (const QFileInfo &fileInfo : files) {
        // Apply filter if extensions are specified
        if (!allowedExtensions.isEmpty()) {
            QString ext = fileInfo.suffix().toLower();
            if (!allowedExtensions.contains(ext)) {
                continue;  // Skip this file
            }
        }
        
        QTreeWidgetItem *fileItem = new QTreeWidgetItem(parent);
        fileItem->setText(0, fileInfo.fileName());
        fileItem->setIcon(0, getIconForFile(fileInfo.fileName()));
        fileItem->setData(0, Qt::UserRole, fileInfo.absoluteFilePath());
    }
}

QIcon AssetBrowser::getIconForFile(const QString &fileName)
{
    if (fileName.endsWith(".prg")) {
        return QIcon::fromTheme("text-x-script");
    } else if (fileName.endsWith(".raymap")) {
        return QIcon::fromTheme("map");
    } else if (fileName.endsWith(".md3")) {
        return QIcon::fromTheme("applications-graphics");
    } else if (fileName.endsWith(".fpg")) {
        return QIcon::fromTheme("image-x-generic");
    } else if (fileName.endsWith(".png") || fileName.endsWith(".jpg")) {
        return QIcon::fromTheme("image-x-generic");
    } else if (fileName.endsWith(".wav") || fileName.endsWith(".ogg")) {
        return QIcon::fromTheme("audio-x-generic");
    } else if (fileName.endsWith(".eff")) {
        return QIcon::fromTheme("preferences-desktop-effects");
    }
    
    return QIcon::fromTheme("text-x-generic");
}

void AssetBrowser::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    
    QString filePath = item->data(0, Qt::UserRole).toString();
    QFileInfo fileInfo(filePath);
    
    // Only emit signal for files, not directories
    if (fileInfo.isFile()) {
        qDebug() << "AssetBrowser: File clicked:" << filePath;
        emit fileClicked(filePath);
    }
}


void AssetBrowser::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    
    QString filePath = item->data(0, Qt::UserRole).toString();
    QFileInfo fileInfo(filePath);
    
    // Only emit signal for files, not directories
    if (fileInfo.isFile()) {
        qDebug() << "File double-clicked:" << filePath;
        emit fileDoubleClicked(filePath);
    }
}

void AssetBrowser::onDirectoryChanged(const QString &path)
{
    Q_UNUSED(path);
    refresh();
}

void AssetBrowser::onContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_treeWidget->itemAt(pos);
    if (!item) return;
    
    QString path = item->data(0, Qt::UserRole).toString();
    QFileInfo info(path);
    
    m_selectedPath = path;
    
    QMenu menu(this);
    
    qDebug() << "Context menu for:" << path << "isFile:" << info.isFile() << "isDir:" << info.isDir();
    
    // Option to open .raymap files
    if (info.isFile() && info.suffix().toLower() == "raymap") {
        qDebug() << "Adding 'Abrir Mapa' option";
        QAction *openMapAction = menu.addAction(QIcon::fromTheme("document-open"), "Abrir Mapa");
        connect(openMapAction, &QAction::triggered, this, [this]() {
            qDebug() << "Opening map:" << m_selectedPath;
            emit mapFileRequested(m_selectedPath);
        });
        menu.addSeparator();
    }
    
    // Option to open .fpg files
    if (info.isFile() && info.suffix().toLower() == "fpg") {
        QAction *openFPGAction = menu.addAction(QIcon::fromTheme("image-x-generic"), "Abrir en Editor FPG");
        connect(openFPGAction, &QAction::triggered, this, [this]() {
            emit fpgEditorRequested(m_selectedPath);
        });
        menu.addSeparator();
    }
    
    // Folder operations
    if (info.isDir()) {
        // Don't allow renaming src folder
        if (!path.endsWith("/src")) {
            QAction *renameAction = menu.addAction(QIcon::fromTheme("edit-rename"), "Renombrar");
            connect(renameAction, &QAction::triggered, this, &AssetBrowser::onRenameFolder);
        }
        
        QAction *createAction = menu.addAction(QIcon::fromTheme("folder-new"), "Nueva Carpeta");
        connect(createAction, &QAction::triggered, this, &AssetBrowser::onCreateFolder);
        
        menu.addSeparator();
    }
    
    QAction *addFileAction = menu.addAction(QIcon::fromTheme("document-new"), "Añadir Archivo...");
    connect(addFileAction, &QAction::triggered, this, &AssetBrowser::onAddFile);
    
    QAction *newCodeAction = menu.addAction(QIcon::fromTheme("text-x-script"), "Nuevo Código (.prg)...");
    connect(newCodeAction, &QAction::triggered, this, &AssetBrowser::onNewCode);
    
    // Don't allow deleting src, assets, or build folders (only for directories)
    if (info.isDir() && !path.endsWith("/src") && !path.endsWith("/assets") && !path.endsWith("/build")) {
        menu.addSeparator();
        QAction *deleteAction = menu.addAction(QIcon::fromTheme("edit-delete"), "Eliminar Carpeta");
        connect(deleteAction, &QAction::triggered, this, &AssetBrowser::onDeleteFolder);
    }
    
    menu.exec(m_treeWidget->mapToGlobal(pos));
}

void AssetBrowser::onRenameFolder()
{
    if (m_selectedPath.isEmpty()) return;
    
    QFileInfo info(m_selectedPath);
    QString oldName = info.fileName();
    QString parentPath = info.absolutePath();
    
    bool ok;
    QString newName = QInputDialog::getText(this, "Renombrar Carpeta",
        "Nuevo nombre:", QLineEdit::Normal, oldName, &ok);
    
    if (!ok || newName.isEmpty() || newName == oldName) {
        return;
    }
    
    // Validate name
    if (newName.contains('/') || newName.contains('\\')) {
        QMessageBox::warning(this, "Error", "El nombre no puede contener / o \\");
        return;
    }
    
    QDir dir(parentPath);
    if (dir.rename(oldName, newName)) {
        refresh();
        QMessageBox::information(this, "Éxito", 
            QString("Carpeta renombrada de '%1' a '%2'").arg(oldName, newName));
    } else {
        QMessageBox::warning(this, "Error", 
            "No se pudo renombrar la carpeta. Verifica que no esté en uso.");
    }
}

void AssetBrowser::onCreateFolder()
{
    if (m_selectedPath.isEmpty()) return;
    
    bool ok;
    QString folderName = QInputDialog::getText(this, "Nueva Carpeta",
        "Nombre de la carpeta:", QLineEdit::Normal, "nueva_carpeta", &ok);
    
    if (!ok || folderName.isEmpty()) {
        return;
    }
    
    // Validate name
    if (folderName.contains('/') || folderName.contains('\\')) {
        QMessageBox::warning(this, "Error", "El nombre no puede contener / o \\");
        return;
    }
    
    QDir dir(m_selectedPath);
    if (dir.mkdir(folderName)) {
        refresh();
        QMessageBox::information(this, "Éxito", 
            QString("Carpeta '%1' creada exitosamente").arg(folderName));
    } else {
        QMessageBox::warning(this, "Error", 
            "No se pudo crear la carpeta. Puede que ya exista.");
    }
}

void AssetBrowser::onDeleteFolder()
{
    if (m_selectedPath.isEmpty()) return;
    
    QFileInfo info(m_selectedPath);
    QString folderName = info.fileName();
    
    // Confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(this, 
        "Confirmar Eliminación",
        QString("¿Estás seguro de que quieres eliminar la carpeta '%1' y todo su contenido?\n\n"
                "Esta acción no se puede deshacer.").arg(folderName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    QDir dir(m_selectedPath);
    if (dir.removeRecursively()) {
        refresh();
        QMessageBox::information(this, "Éxito", 
            QString("Carpeta '%1' eliminada exitosamente").arg(folderName));
    } else {
        QMessageBox::warning(this, "Error", 
            "No se pudo eliminar la carpeta. Verifica que no esté en uso.");
    }
}

void AssetBrowser::addDirectoryToWatcher(const QString &path)
{
    QDir dir(path);
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString &subdir : subdirs) {
        QString subdirPath = path + "/" + subdir;
        m_watcher->addPath(subdirPath);
        addDirectoryToWatcher(subdirPath);  // Recursive
    }
}

void AssetBrowser::onAddFile()
{
    if (m_selectedPath.isEmpty()) return;
    
    QString filePath = QFileDialog::getOpenFileName(this, "Seleccionar Archivo",
        QDir::homePath(), "Todos los archivos (*)");
    
    if (filePath.isEmpty()) return;
    
    QFileInfo sourceFile(filePath);
    QString destPath = m_selectedPath + "/" + sourceFile.fileName();
    
    if (QFile::exists(destPath)) {
        QMessageBox::warning(this, "Error", 
            QString("El archivo '%1' ya existe en esta carpeta.").arg(sourceFile.fileName()));
        return;
    }
    
    if (QFile::copy(filePath, destPath)) {
        refresh();
        QMessageBox::information(this, "Éxito", 
            QString("Archivo '%1' añadido exitosamente").arg(sourceFile.fileName()));
    } else {
        QMessageBox::warning(this, "Error", "No se pudo copiar el archivo.");
    }
}

void AssetBrowser::onNewCode()
{
    if (m_selectedPath.isEmpty()) return;
    
    bool ok;
    QString fileName = QInputDialog::getText(this, "Nuevo Archivo de Código",
        "Nombre del archivo (sin extensión):", QLineEdit::Normal, "nuevo_proceso", &ok);
    
    if (!ok || fileName.isEmpty()) return;
    
    if (!fileName.endsWith(".prg")) {
        fileName += ".prg";
    }
    
    QString filePath = m_selectedPath + "/" + fileName;
    
    if (QFile::exists(filePath)) {
        QMessageBox::warning(this, "Error", "El archivo ya existe.");
        return;
    }
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << "// " << fileName << "\n";
        out << "// Auto-generado por RayMap Editor\n\n";
        out << "PROCESS mi_proceso()\n";
        out << "BEGIN\n";
        out << "    LOOP\n";
        out << "        // Tu código aquí\n";
        out << "        FRAME;\n";
        out << "    END\n";
        out << "END\n";
        file.close();
        
        refresh();
        QMessageBox::information(this, "Éxito", 
            QString("Archivo '%1' creado exitosamente").arg(fileName));
    } else {
        QMessageBox::warning(this, "Error", "No se pudo crear el archivo.");
    }
}
