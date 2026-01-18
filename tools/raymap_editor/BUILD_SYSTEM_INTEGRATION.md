# Build & Run System - Integration Checklist

## ✅ Archivos Creados:
1. buildmanager.h / buildmanager.cpp - Gestión de build y run
2. consolewidget.h / consolewidget.cpp - Widget de consola
3. bennugdinstaller.h / bennugdinstaller.cpp - Instalador automático
4. mainwindow_build.cpp - Implementación de build/run en MainWindow
5. mainwindow_build_ui_snippet.txt - Código para añadir al constructor

## ✅ Archivos Modificados:
1. mainwindow.h - Añadidos slots y miembros
2. CMakeLists.txt - Añadidos archivos y Qt Network

## ⚠️ Pendiente de Integración Manual:

### 1. En mainwindow.h, añadir en la sección private:
```cpp
private:
    void setupBuildSystem();  // Add this declaration
```

### 2. En mainwindow.cpp constructor, añadir después de setupUI():
```cpp
// Setup Build System
setupBuildSystem();

// Add Build & Run toolbar
QToolBar *buildToolbar = addToolBar(tr("Build"));
buildToolbar->setObjectName("BuildToolbar");

QAction *buildAction = buildToolbar->addAction(QIcon::fromTheme("system-run"), tr("Build"));
connect(buildAction, &QAction::triggered, this, &MainWindow::onBuildProject);

QAction *runAction = buildToolbar->addAction(QIcon::fromTheme("media-playback-start"), tr("Run"));
connect(runAction, &QAction::triggered, this, &MainWindow::onRunProject);

QAction *buildAndRunAction = buildToolbar->addAction(QIcon::fromTheme("system-run"), tr("Build & Run"));
buildAndRunAction->setShortcut(QKeySequence(Qt::Key_F5));
connect(buildAndRunAction, &QAction::triggered, this, &MainWindow::onBuildAndRun);

buildToolbar->addSeparator();

QAction *stopAction = buildToolbar->addAction(QIcon::fromTheme("process-stop"), tr("Stop"));
stopAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
connect(stopAction, &QAction::triggered, this, &MainWindow::onStopRunning);

// Add to Project menu
m_projectMenu->addSeparator();
m_projectMenu->addAction(buildAndRunAction);
m_projectMenu->addAction(buildAction);
m_projectMenu->addAction(runAction);
m_projectMenu->addAction(stopAction);

// Add Install BennuGD2 to Help menu
QMenu *helpMenu = menuBar()->addMenu(tr("Help"));
QAction *installBennuGD2Action = helpMenu->addAction(tr("Install BennuGD2..."));
connect(installBennuGD2Action, &QAction::triggered, this, &MainWindow::onInstallBennuGD2);
```

## 📋 Funcionalidades Implementadas:

1. **Detección Automática de BennuGD2**
   - Busca en PATH
   - Busca en ubicaciones comunes
   - Busca en instalación local (~/.bennugd2)

2. **Descarga e Instalación Automática**
   - Obtiene última release desde GitHub API
   - Descarga binarios para el OS correcto
   - Barra de progreso visual
   - Extracción e instalación automática

3. **Build System**
   - Compilación con bgdc
   - Salida en consola con colores
   - Detección de errores

4. **Run System**
   - Ejecución con bgdi
   - Captura de salida en tiempo real
   - Botón de Stop para terminar ejecución

5. **UI**
   - Toolbar con botones Build, Run, Build & Run, Stop
   - Consola dockable en la parte inferior
   - Atajos de teclado (F5 = Build & Run, Shift+F5 = Stop)
   - Menú Help con opción de instalar BennuGD2

## 🚀 Uso:

1. **Primera vez (sin BennuGD2):**
   - Al abrir el editor, pregunta si quieres instalar BennuGD2
   - Descarga e instala automáticamente
   
2. **Build:**
   - Click en "Build" o Ctrl+B
   - Compila src/main.prg → game.dcb
   
3. **Run:**
   - Click en "Run"
   - Ejecuta game.dcb desde src/
   
4. **Build & Run:**
   - Click en "Build & Run" o F5
   - Compila y ejecuta automáticamente
   
5. **Stop:**
   - Click en "Stop" o Shift+F5
   - Termina el juego en ejecución
