#pragma once

#include <QWidget>
#include <QPushButton>
#include <QTimer>
#include <QPaintEvent>
#include <QKeyEvent>
#include "aur/project.hpp"
#include "aur/game.hpp"
#include "playtest/QtRenderer.hpp"

namespace aur {

class SharedAudio;

class GameView : public QWidget {
    Q_OBJECT
    Project project;
    Game game;
    QtRenderer renderer;
    QTimer timer;
    InputState input_state;
    bool debugHud = false;
    SharedAudio* shared = nullptr;
    QWidget* audioOverlay = nullptr;
    QPushButton* soundButton = nullptr;
    std::string songMapId;
    bool pageVisible = false;
    bool soundOn = false;

    // The sound of the map the game is on, through the studio's one device.
    void syncSong();
    void buildAudioOverlay();

public:
    explicit GameView(const std::string& project_path, QWidget* parent = nullptr,
                      SharedAudio* shared = nullptr);
    void reloadProject();
    void loadProject(const std::string& project_path);
    void setProject(const Project& source);
    void syncProject(const Project& source);
    // Plays a named map, for the render sweep and the world tab's hand-off.
    void setMap(const std::string& map_id);
    void openDebugMenu();
    // Plays the map's song, for the chrome's play button.
    void setSound(bool on);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
};

} // namespace aur
