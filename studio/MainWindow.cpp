#include "MainWindow.hpp"
#include "core/AurSettings.hpp"
#include "music/SharedAudio.hpp"
#include "MapEditorPanel.hpp"
#include "DatabasePanel.hpp"
#include "CreatureForgePanel.hpp"
#include "MusicPanel.hpp"
#include "WorldPanel.hpp"
#include "SpriteEditorPanel.hpp"
#include "EventsPanel.hpp"
#include "TilesetEditorPanel.hpp"
#include "world/WorldSettingsPanel.hpp"
#include "theme/FramelessWindow.hpp"
#include "theme/StudioChrome.hpp"
#include "theme/AppearanceDialog.hpp"
#include "theme/ThemeIcons.hpp"
#include "theme/ThemeManager.hpp"
#include <QLabel>
#include <QApplication>
#include <QSettings>
#include <QShortcut>
#include <QMenu>
#include <QIcon>
#include <QKeySequence>
#include <QMessageBox>
#include <QFileDialog>
#include <QStyle>
#include <QStatusBar>
#include <functional>

namespace aur {

MainWindow::MainWindow(const std::string& project_path, QWidget* parent)
    : QMainWindow(parent), project(Project::load(project_path)), project_path(project_path) {
    saved_signature = projectSignature();
    // One audio device for the app: the DAW's preview and the playtest's
    // music share it, so swapping tabs stops the other.
    sharedAudio = new SharedAudio(this);
    updateStudioTitle();
    resize(1600, 1000);
    setMinimumSize(1280, 690);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    auto* chrome = new StudioChrome(this);
    chromeTitle = chrome->findChild<QLabel*>("StudioTitle");

    QMenu* fileMenu = chrome->fileMenu();
    QAction* openAction = fileMenu->addAction("&Open Project…");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openProject);
    saveAction = fileMenu->addAction("&Save Project");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);
    fileMenu->addSeparator();
    reloadAction = fileMenu->addAction("&Reload from Disk");
    connect(reloadAction, &QAction::triggered, this, &MainWindow::reloadProject);
    QAction* appearanceAction = fileMenu->addAction(theme::icon(theme::Glyph::Appearance), "&Appearance…");
    connect(appearanceAction, &QAction::triggered, this, [this]() {
        theme::AppearanceDialog(this).exec();
    });
    // The project's actions are icon buttons in the title row, beside the
    // hamburger: one control cluster, and the editor gets the row back.
    QList<QAction*> project_actions;
    bool compact_buttons = true;
    auto addProjectAction = [this, &project_actions](const QString& text, const QString& shortText,
                                   const QString& tip, const QIcon& icon,
                                   const std::function<void()>& callback) {
        QAction* action = new QAction(icon, text, this);
        action->setIconText(shortText);
        action->setToolTip(tip);
        connect(action, &QAction::triggered, this, callback);
        project_actions.append(action);
        return action;
    };
    QAction* saveToolbarAction = addProjectAction("Save Project", "Save", "Save project (Ctrl+S)",
                     theme::icon(theme::Glyph::Save),
                     [this]() { saveProject(); });
    connect(saveToolbarAction, &QAction::triggered, saveAction, [this]() {
        saveAction->trigger();
    });
    QAction* reloadToolbarAction = addProjectAction("Reload Agent Changes", "Reload", "Reload agent changes from disk",
                     theme::icon(theme::Glyph::Reload),
                     [this]() { reloadProject(); });
    connect(reloadToolbarAction, &QAction::triggered, reloadAction, [this]() {
        reloadAction->trigger();
    });
    addProjectAction("Play Project", "Play", "Play project (F5)",
                     theme::icon(theme::Glyph::Play),
                     [this]() { playProject(); });
    addProjectAction("Pause", "Pause", "Pause project",
                     theme::icon(theme::Glyph::Pause),
                     [this]() { pauseProject(); });
    addProjectAction("Stop", "Stop", "Stop project",
                     theme::icon(theme::Glyph::Stop),
                     [this]() { stopProject(); });
    addProjectAction("Appearance", "Theme", "Theme and accent colour",
                     theme::icon(theme::Glyph::Appearance),
                     [this]() { theme::AppearanceDialog(this).exec(); });
    chrome->setProjectActions(project_actions);
    auto* compactToolbarShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Alt | Qt::Key_T), this);
    connect(compactToolbarShortcut, &QShortcut::activated, this, [chrome, &compact_buttons]() {
        chrome->setCompactButtons(!compact_buttons);
        compact_buttons = !compact_buttons;
    });
    chrome->setToolTip("Ctrl+Alt+T toggles the project buttons' text");

    // Main Tabbed Editor Workspace: the tab bar sits in the title row
    // beside the project's buttons, and the pages are the stack below.
    tabs = new QTabBar(this);
    tabs->setExpanding(false);
    tabs->setUsesScrollButtons(true);
    tabs->setElideMode(Qt::ElideRight);
    tabStack = new QStackedWidget(this);
    connect(tabs, &QTabBar::currentChanged, tabStack, &QStackedWidget::setCurrentIndex);
    chrome->addTitleWidget(tabs);
    gameView = new GameView(project_path, this, sharedAudio);
    addTab(gameView, "▶ Playtest");
    auto* mapEditor = new MapEditorPanel(&project, this);
    addTab(mapEditor, "Map Editor");    auto* tilesetEditor = new TilesetEditorPanel(&project, this);
    addTab(tilesetEditor, "Tileset Editor");
    // Flag edits in the tileset editor invalidate the map editor's tile cache
    // and repaint its collision overlay (editsweep D1.S5).
    connect(tilesetEditor, &TilesetEditorPanel::tilesetFlagsChanged,
            mapEditor, &MapEditorPanel::onTilesetChanged);
    auto* worldPanel = new WorldPanel(&project, this);
    addTab(worldPanel, "World");
    // An expansion grows a map; the map editor opens it.
    connect(worldPanel, &WorldPanel::mapExpanded, this, [this](const QString& id) {
        if (tabs)
        {
            for (int index = 0; index < tabs->count(); ++index)
            {
                if (tabs->tabText(index).contains("Map Editor", Qt::CaseInsensitive))
                {
                    tabs->setCurrentIndex(index);
                    break;
                }
            }
        }
        if (auto* editor = findChild<MapEditorPanel*>()) editor->openMap(id);
    });
    addTab(new SpriteEditorPanel(&project, this), "Sprite Editor");
    addTab(new CreatureForgePanel(&project, this), "Creature Forge");
    addTab(new DatabasePanel(&project, this), "Database");
    addTab(new EventsPanel(&project, this), "Events");
    addTab(new MusicPanel(&project, sharedAudio, this), "DAW");
    // World & Time Settings is baked into the World tab (WANTED #3).

    auto installEditorMenu = [this](QWidget* canvas, const std::function<void()>& undo,
                                      const std::function<void()>& redo,
                                      const std::function<void()>& zoomIn,
                                      const std::function<void()>& zoomOut,
                                      const std::function<void()>& fit) {
        auto* menu = new QMenu(canvas);
        QAction* undoAction = menu->addAction("Undo");
        QAction* redoAction = menu->addAction("Redo");
        menu->addSeparator();
        QAction* zoomInAction = menu->addAction("Zoom In");
        QAction* zoomOutAction = menu->addAction("Zoom Out");
        QAction* fitAction = menu->addAction("Fit to View");
        connect(undoAction, &QAction::triggered, canvas, undo);
        connect(redoAction, &QAction::triggered, canvas, redo);
        connect(zoomInAction, &QAction::triggered, canvas, zoomIn);
        connect(zoomOutAction, &QAction::triggered, canvas, zoomOut);
        connect(fitAction, &QAction::triggered, canvas, fit);
        canvas->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(canvas, &QWidget::customContextMenuRequested, this,
                [canvas, menu](const QPoint& point) { menu->popup(canvas->mapToGlobal(point)); });
    };
    for (int index = 0; index < tabs->count(); ++index) {
        QWidget* page = tabStack->widget(index);
        if (auto* mapCanvas = page->findChild<MapCanvasWidget*>()) {
            installEditorMenu(mapCanvas,
                [mapCanvas]() { mapCanvas->undo(); },
                [mapCanvas]() { mapCanvas->redo(); },
                [mapCanvas]() { mapCanvas->zoomIn(); },
                [mapCanvas]() { mapCanvas->zoomOut(); },
                [mapCanvas]() { mapCanvas->fitToView(); });
        }
        if (auto* worldCanvas = page->findChild<WorldMapCanvasWidget*>()) {
            installEditorMenu(worldCanvas,
                [worldCanvas]() { worldCanvas->undo(); },
                [worldCanvas]() { worldCanvas->redo(); },
                [worldCanvas]() { worldCanvas->setZoom(worldCanvas->zoomFactor() * 1.25); },
                [worldCanvas]() { worldCanvas->setZoom(worldCanvas->zoomFactor() / 1.25); },
                [worldCanvas]() { worldCanvas->fitToView(); });
        }
    }

    setCentralWidget(tabStack);

    // Keyboard Shortcuts (F5 Play; Ctrl+S is the File-menu action)
    QShortcut* f5Shortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(f5Shortcut, &QShortcut::activated, this, &MainWindow::playProject);

    // F11 and Alt+Enter swap between maximized and windowed.
    auto toggleWindowMode = [this]() {
        if (isMaximized()) showNormal();
        else showMaximized();
    };
    QShortcut* fullScreenShortcut = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(fullScreenShortcut, &QShortcut::activated, this, toggleWindowMode);
    QShortcut* altReturnShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Return), this);
    connect(altReturnShortcut, &QShortcut::activated, this, toggleWindowMode);
    QShortcut* altEnterShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Enter), this);
    connect(altEnterShortcut, &QShortcut::activated, this, toggleWindowMode);
    connect(&theme::ThemeManager::instance(), &theme::ThemeManager::changed, this, [this]() {
        for (QWidget* w : findChildren<QWidget*>()) w->update();
        update();
    });

    // Ctrl+Tab walks the pages, and Ctrl+Shift+Tab walks back.
    QShortcut* nextPage = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), this);
    connect(nextPage, &QShortcut::activated, this, [this]() {
        if (tabs && tabs->count() > 0)
            tabs->setCurrentIndex((tabs->currentIndex() + 1) % tabs->count());
    });
    QShortcut* previousPage = new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), this);
    connect(previousPage, &QShortcut::activated, this, [this]() {
        if (tabs && tabs->count() > 0)
            tabs->setCurrentIndex((tabs->currentIndex() - 1 + tabs->count()) % tabs->count());
    });
}

void MainWindow::saveProject() {
    try {
        project.save(project_path);
        saved_signature = projectSignature();
        statusBar()->showMessage("Project saved", 3000);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Save Error", QString("Failed to save project: ") + e.what());
    }
}

void MainWindow::openProject() {
    const QString selected_path = QFileDialog::getExistingDirectory(
        this, "Open Aur Project", QString::fromStdString(project_path),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (selected_path.isEmpty()) return;

    try {
        Project opened = Project::load(selected_path.toStdString());
        if (projectSignature() != saved_signature) {
            const auto choice = QMessageBox::warning(
                this, "Open Project",
                "Save changes to the current project before opening another?",
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
            if (choice == QMessageBox::Cancel) return;
            if (choice == QMessageBox::Save) {
                try {
                    project.save(project_path);
                } catch (const std::exception& error) {
                    QMessageBox::critical(this, "Save Error",
                                           QString("Could not save the current project: ") +
                                               error.what());
                    return;
                }
            }
        }
        project = std::move(opened);
        project_path = selected_path.toStdString();
        saved_signature = projectSignature();
        refreshProjectPanels();
        updateStudioTitle();
        statusBar()->showMessage("Opened " + selected_path, 5000);
    } catch (const std::exception& error) {
        QMessageBox::critical(this, "Open Project Error",
                              QString("Could not open project: ") + error.what());
    }
}

std::string MainWindow::projectSignature() const {
    return dump_json(Json(JsonObject{
        {"meta", project.meta}, {"tilesets", project.tilesets}, {"graphics", project.graphics},
        {"characters", project.characters}, {"db", project.db}, {"events", project.events},
        {"parts", project.parts}, {"biomes", project.biomes}, {"maps", project.maps},
        {"music", project.music}
    }));
}

void MainWindow::addTab(QWidget* page, const QString& title) {
    tabStack->addWidget(page);
    tabs->addTab(title);
}

void MainWindow::setSubTab(const QString& name) {
    if (!tabs || !tabStack) return;
    QWidget* page = tabStack->widget(tabs->currentIndex());
    if (!page) return;
    // The pages that have tabs of their own answer this one.
    QMetaObject::invokeMethod(page, "selectPageTab", Q_ARG(QString, name));
}

void MainWindow::updateStudioTitle() {
    const QString title = "Aur";
    setWindowTitle(title);
    if (chromeTitle) chromeTitle->setText(title);
}

// Window state (the last open tab) lives in ~/.Aur; window geometry is not
// restored because the studio always opens maximized.
void MainWindow::restoreWindowState(bool restore_geometry, bool restore_tab) {
    Q_UNUSED(restore_geometry);
    AurSettings::instance().load();
    if (restore_tab) {
        const int tab = AurSettings::instance().windowTab();
        if (tabs && tab >= 0 && tab < tabs->count()) tabs->setCurrentIndex(tab);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (save_window_state) {
        AurSettings::instance().setWindowTab(tabs ? tabs->currentIndex() : 0);
    }
    QMainWindow::closeEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    if (handleFramelessNativeEvent(this, eventType, message, result)) return true;
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool MainWindow::selectTab(const QString& name_or_index) {
    if (!tabs || name_or_index.isEmpty()) return false;
    bool numeric = false;
    const int index = name_or_index.toInt(&numeric);
    if (numeric) {
        if (index < 0 || index >= tabs->count()) return false;
        tabs->setCurrentIndex(index);
        return true;
    }
    for (int i = 0; i < tabs->count(); ++i) {
        if (tabs->tabText(i).compare(name_or_index, Qt::CaseInsensitive) == 0) {
            tabs->setCurrentIndex(i);
            return true;
        }
    }
    return false;
}

void MainWindow::setEditorZoom(qreal factor) {
    if (!tabs) return;
    QWidget* page = tabStack->widget(tabs->currentIndex());
    if (auto* map_editor = qobject_cast<MapEditorPanel*>(page))
        map_editor->setEditorZoom(factor);
    else if (auto* world = qobject_cast<WorldPanel*>(page))
        world->setEditorZoom(factor);
}

void MainWindow::setSpriteMode(bool enabled) {
    for (auto* panel : findChildren<SpriteEditorPanel*>())
        panel->setSpriteMode(enabled);
}

void MainWindow::startPreview() {
    for (auto* panel : findChildren<SpriteEditorPanel*>())
        panel->startPreview();
}

void MainWindow::refreshProjectPanels() {
    for (auto* panel : findChildren<MapEditorPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<SpriteEditorPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<TilesetEditorPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<CreatureForgePanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<DatabasePanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<EventsPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<MusicPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<WorldPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<WorldSettingsPanel*>()) panel->setProject(&project);
    if (gameView) gameView->setProject(project);
}

void MainWindow::reloadProject() {
    const auto answer = QMessageBox::warning(
        this, "Reload Project",
        "Reload project files from disk? Unsaved Studio edits will be discarded.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    try {
        Project reloaded = Project::load(project_path);
        project = std::move(reloaded);
        saved_signature = projectSignature();
        updateStudioTitle();
        refreshProjectPanels();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Reload Error", QString("Failed to reload project: ") + e.what());
    }
}

void MainWindow::playProject() {
    if (tabs && gameView) {
        gameView->syncProject(project);
        tabs->setCurrentIndex(tabStack->indexOf(gameView));
        gameView->setFocus();
        // Asking to play the project asks for its sound too.
        gameView->setSound(true);
    }
}

void MainWindow::pauseProject() {
    // Toggle pause state
}

void MainWindow::stopProject() {
    if (tabs) {
        tabs->setCurrentIndex(1); // Return to Map Editor
    }
}

} // namespace aur
