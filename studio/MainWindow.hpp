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

class MainWindow : public QMainWindow {
    Q_OBJECT
    Project project;
    std::string project_path;
    std::string saved_signature;
    QTabWidget* tabs = nullptr;
    GameView* gameView = nullptr;
    QPushButton* playBtn = nullptr;
    QPushButton* pauseBtn = nullptr;
    QPushButton* stopBtn = nullptr;
    QPushButton* saveBtn = nullptr;
    QPushButton* reloadBtn = nullptr;
    QLabel* chromeTitle = nullptr;

    void refreshProjectPanels();
    std::string projectSignature() const;
    void updateStudioTitle();

public:
    explicit MainWindow(const std::string& project_path = "projects/sylvi", QWidget* parent = nullptr);

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
