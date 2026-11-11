#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QAction>
#include <QByteArray>
#include <QMessageBox>
#include "aur/project.hpp"
#include "GameView.hpp"

namespace aur {

class PixelEditorPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
    Project project;
    std::string project_path;
    std::string saved_signature;
    QTabWidget* tabs = nullptr;
    GameView* gameView = nullptr;
    QLabel* chromeTitle = nullptr;
    QAction* saveAction = nullptr;
    QAction* reloadAction = nullptr;
    QAction* playAction = nullptr;
    QAction* pauseAction = nullptr;
    QAction* stopAction = nullptr;

    void refreshProjectPanels();
    std::string projectSignature() const;
    void updateStudioTitle();

public:
    explicit MainWindow(const std::string& project_path = "projects/sylvi", QWidget* parent = nullptr);
    // Selects a tab by title or index. Returns false when there is no such
    // tab, so a script can say so instead of capturing the wrong page.
    bool selectTab(const QString& name_or_index);
    // Applies the zoom control to the current page's canvas, as a factor of
    // its fit.
    void setEditorZoom(qreal factor);
    // Puts the pixel editor in sprite mode, for the render sweep.
    void setSpriteMode(bool enabled);

public slots:
    void playProject();
    void pauseProject();
    void stopProject();
    void saveProject();
    void reloadProject();
    void openProject();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
};

} // namespace aur
