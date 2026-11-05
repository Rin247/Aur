#include "MainWindow.hpp"
#include "MapEditorPanel.hpp"
#include "DatabasePanel.hpp"
#include "ForgePanel.hpp"
#include "MusicPanel.hpp"
#include "WorldPanel.hpp"
#include "PixelEditorPanel.hpp"
#include "EventPanel.hpp"
#include "TilesetEditorPanel.hpp"
#include "WorldSettingsPanel.hpp"
#include <QLabel>
#include <QShortcut>
#include <QKeySequence>
#include <QMessageBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStyle>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>
#include <QEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

namespace aur {
namespace {

class StudioChrome final : public QWidget {
    QMainWindow* window;
    QPoint dragPosition;
    bool dragging = false;
    QToolButton* maximizeButton = nullptr;

public:
    explicit StudioChrome(QMainWindow* owner) : QWidget(owner), window(owner) {
        setObjectName("StudioChrome");
        setStyleSheet("#StudioChrome { background:#11161e; border-bottom:1px solid #39414b; }");
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 4, 8, 0);
        layout->setSpacing(2);
        auto* titleRow = new QHBoxLayout();
        titleRow->setContentsMargins(4, 0, 0, 0);
        auto* titleLabel = new QLabel(owner->windowTitle(), this);
        titleLabel->setObjectName("StudioTitle");
        titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        titleRow->addWidget(titleLabel);
        titleRow->addStretch();
        auto* minimize = new QToolButton(this);
        minimize->setIcon(style()->standardIcon(QStyle::SP_TitleBarMinButton));
        minimize->setToolTip("Minimize");
        titleRow->addWidget(minimize);
        maximizeButton = new QToolButton(this);
        maximizeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
        maximizeButton->setToolTip("Maximize or restore");
        titleRow->addWidget(maximizeButton);
        auto* close = new QToolButton(this);
        close->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        close->setToolTip("Close Aur Studio");
        close->setStyleSheet("QToolButton:hover { background:#8d3f3f; }");
        titleRow->addWidget(close);
        layout->addLayout(titleRow);

        auto* qtMenuBar = new QMenuBar(this);
        qtMenuBar->setNativeMenuBar(false);
        layout->addWidget(qtMenuBar);
        owner->setMenuWidget(this);
        QObject::connect(minimize, &QToolButton::clicked, owner, &QWidget::showMinimized);
        QObject::connect(maximizeButton, &QToolButton::clicked, owner, [owner]() {
            if (owner->isMaximized()) {
                owner->showNormal();
            } else {
                owner->showMaximized();
            }
        });
        QObject::connect(close, &QToolButton::clicked, owner, &QWidget::close);
        owner->installEventFilter(this);
    }

    QMenuBar* menuBar() const {
        return findChild<QMenuBar*>();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == window && event->type() == QEvent::WindowStateChange && maximizeButton) {
            maximizeButton->setIcon(window->style()->standardIcon(
                window->isMaximized() ? QStyle::SP_TitleBarNormalButton
                                      : QStyle::SP_TitleBarMaxButton));
        }
        return QWidget::eventFilter(watched, event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window && !window->isMaximized()) {
            dragging = true;
            dragPosition = event->globalPosition().toPoint() - window->frameGeometry().topLeft();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (dragging && window && (event->buttons() & Qt::LeftButton) && !window->isMaximized()) {
            window->move(event->globalPosition().toPoint() - dragPosition);
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        dragging = false;
        QWidget::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && window) {
            window->isMaximized() ? window->showNormal() : window->showMaximized();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }
};

} // namespace

MainWindow::MainWindow(const std::string& project_path, QWidget* parent)
    : QMainWindow(parent), project(Project::load(project_path)), project_path(project_path) {
    saved_signature = projectSignature();
    updateStudioTitle();
    resize(1600, 1000);
    setMinimumSize(1280, 720);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    auto* chrome = new StudioChrome(this);
    chromeTitle = chrome->findChild<QLabel*>("StudioTitle");

    QMenu* fileMenu = chrome->menuBar()->addMenu("&File");
    QAction* openAction = fileMenu->addAction("&Open Project…");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openProject);
    QAction* saveAction = fileMenu->addAction("&Save Project");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveProject);
    fileMenu->addSeparator();
    QAction* reloadAction = fileMenu->addAction("&Reload from Disk");
    connect(reloadAction, &QAction::triggered, this, &MainWindow::reloadProject);
    // Playtest & Save Toolbar
    QToolBar* toolbar = addToolBar("Playtest");
    toolbar->setMovable(false);

    saveBtn = new QPushButton("💾 Save Project (Ctrl+S)", this);
    saveBtn->setStyleSheet("background-color: #1976d2; color: white; font-weight: bold; padding: 4px 12px;");
    toolbar->addWidget(saveBtn);

    reloadBtn = new QPushButton("Reload Agent Changes", this);
    toolbar->addWidget(reloadBtn);

    toolbar->addSeparator();

    playBtn = new QPushButton("▶ Play Project (F5)", this);
    playBtn->setStyleSheet("background-color: #2e7d32; color: white; font-weight: bold; padding: 4px 12px;");
    toolbar->addWidget(playBtn);

    pauseBtn = new QPushButton("⏸ Pause", this);
    toolbar->addWidget(pauseBtn);

    stopBtn = new QPushButton("⏹ Stop", this);
    toolbar->addWidget(stopBtn);
    // Main Tabbed Editor Workspace
    tabs = new QTabWidget(this);
    gameView = new GameView(project_path, this);
    tabs->addTab(gameView, "▶ Playtest View");
    tabs->addTab(new MapEditorPanel(&project, this), "Map Editor");
    tabs->addTab(new PixelEditorPanel(&project, this), "Pixel Editor");
    tabs->addTab(new SpriteEditorPanel(&project, this), "Sprite Editor");
    tabs->addTab(new TilesetEditorPanel(&project, this), "Tileset Editor");
    tabs->addTab(new ForgePanel(&project, this), "Character Forge");
    tabs->addTab(new DatabasePanel(&project, this), "Database");
    tabs->addTab(new EventPanel(&project, this), "Event Scripting");
    tabs->addTab(new MusicPanel(&project, this), "Music Studio");
    tabs->addTab(new WorldPanel(&project, this), "World");
    tabs->addTab(new WorldSettingsPanel(&project, this), "World & Time Settings");

    setCentralWidget(tabs);

    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveProject);
    connect(reloadBtn, &QPushButton::clicked, this, &MainWindow::reloadProject);
    connect(playBtn, &QPushButton::clicked, this, &MainWindow::playProject);
    connect(pauseBtn, &QPushButton::clicked, this, &MainWindow::pauseProject);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::stopProject);
    // Keyboard Shortcuts (F5 Play, Ctrl+S Save)
    QShortcut* f5Shortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(f5Shortcut, &QShortcut::activated, this, &MainWindow::playProject);

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

void MainWindow::updateStudioTitle() {
    const QString title = "Aur Studio";
    setWindowTitle(title);
    if (chromeTitle) chromeTitle->setText(title);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        const auto* native_message = static_cast<MSG*>(message);
        if (native_message->message == WM_GETMINMAXINFO) {
            auto* limits = reinterpret_cast<MINMAXINFO*>(native_message->lParam);
            const HMONITOR monitor = MonitorFromWindow(
                reinterpret_cast<HWND>(winId()), MONITOR_DEFAULTTONEAREST);
            MONITORINFO information{};
            information.cbSize = sizeof(information);
            if (GetMonitorInfoW(monitor, &information)) {
                const RECT& work = information.rcWork;
                const RECT& full = information.rcMonitor;
                limits->ptMaxPosition.x = work.left - full.left;
                limits->ptMaxPosition.y = work.top - full.top;
                limits->ptMaxSize.x = work.right - work.left;
                limits->ptMaxSize.y = work.bottom - work.top;
                *result = 0;
                return true;
            }
        }
        if (native_message->message == WM_NCHITTEST && !isMaximized()) {
            const QPoint position(GET_X_LPARAM(native_message->lParam),
                                  GET_Y_LPARAM(native_message->lParam));
            const QPoint local = mapFromGlobal(position);
            constexpr int border = 8;
            const bool left = local.x() < border;
            const bool right = local.x() >= width() - border;
            const bool top = local.y() < border;
            const bool bottom = local.y() >= height() - border;
            if (top && left) *result = HTTOPLEFT;
            else if (top && right) *result = HTTOPRIGHT;
            else if (bottom && left) *result = HTBOTTOMLEFT;
            else if (bottom && right) *result = HTBOTTOMRIGHT;
            else if (left) *result = HTLEFT;
            else if (right) *result = HTRIGHT;
            else if (top) *result = HTTOP;
            else if (bottom) *result = HTBOTTOM;
            else return QMainWindow::nativeEvent(eventType, message, result);
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::refreshProjectPanels() {
    for (auto* panel : findChildren<MapEditorPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<PixelEditorPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<SpriteEditorPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<TilesetEditorPanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<ForgePanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<DatabasePanel*>()) panel->setProject(&project);
    for (auto* panel : findChildren<EventPanel*>()) panel->setProject(&project);
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
        tabs->setCurrentWidget(gameView);
        gameView->setFocus();
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
