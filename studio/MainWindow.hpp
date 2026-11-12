#pragma once

#include <QMainWindow>
#include <QTabBar>
#include <QStackedWidget>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QAction>
#include <QByteArray>
#include <QMessageBox>
#include <QCloseEvent>
#include "aur/project.hpp"
#include "GameView.hpp"

namespace aur {

class SpriteEditorPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
    Project project;
    std::string project_path;
    std::string saved_signature;
    QTabBar* tabs = nullptr;
    QStackedWidget* tabStack = nullptr;
    GameView* gameView = nullptr;
    QLabel* chromeTitle = nullptr;
    QAction* saveAction = nullptr;
    QAction* reloadAction = nullptr;
    QAction* playAction = nullptr;
    QAction* pauseAction = nullptr;
    QAction* stopAction = nullptr;
    bool save_window_state = true;

    void refreshProjectPanels();
    std::string projectSignature() const;
    void updateStudioTitle();
    // Adds a page to the stack and a tab for it.
    void addTab(QWidget* page, const QString& title);

public:
    explicit MainWindow(const std::string& project_path = "projects/sylvi", QWidget* parent = nullptr);
    // Restores the window's geometry and page from the data root. A caller
    // that asks for a size or a page passes false for it.
    void restoreWindowState(bool restore_geometry = true, bool restore_tab = true);
    // Whether the window's state is written when it closes. The render
    // sweep turns it off, so a capture run does not overwrite the state the
    // person left behind.
    void setSaveWindowState(bool enabled) { save_window_state = enabled; }
    // Selects a tab by title or index. Returns false when there is no such
    // tab, so a script can say so instead of capturing the wrong page.
    bool selectTab(const QString& name_or_index);
    // Applies the zoom control to the current page's canvas, as a factor of
    // its fit.
    void setEditorZoom(qreal factor);
    // Puts the pixel editor in sprite mode, for the render sweep.
    void setSpriteMode(bool enabled);
    // Starts the pixel editor's preview, for the render sweep.
    void startPreview();
    // Shows one of the DAW's own tabs, for the render sweep.
    void setDawTab(const QString &name);

public slots:
    void playProject();
    void pauseProject();
    void stopProject();
    void saveProject();
    void reloadProject();
    void openProject();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;
};

} // namespace aur
