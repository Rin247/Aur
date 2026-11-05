#pragma once

#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QPaintEvent>
#include <QKeyEvent>
#include "aur/project.hpp"
#include "aur/game.hpp"
#include "aur/render.hpp"

namespace aur {

class QtRenderer : public Renderer {
public:
    QPainter* painter = nullptr;
    Project* project = nullptr;
    std::map<std::pair<std::string, std::string>, QImage> tile_cache;
    std::map<std::pair<std::string, int>, QImage> tileset_cache;
    double zoom = 2.0;

    void set_painter(QPainter* p);
    void begin(const std::string& st = "hd2d") override;
    void end() override;
    void camera(int x, int y) override;
    void tile(const std::string&, int index, int x, int y, const std::string& layer = "ground") override;
    void sprite(const std::string& graphic, const std::string& frame, int x, int y, bool flip = false, double scale = 1.0,
                const std::string& tint = "", double alpha = 1.0, bool shadow = true, int z = -1) override;
    void ground_rect(int x, int y, int w, int h, const std::string& color, double alpha = 0.5) override;
    void world_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color = {255,255,255}) override;
    void arc(int x, int y, int radius, double angle, double span_deg,
             const std::tuple<int,int,int>& color = {255,255,255}, double alpha = 0.5) override;
    void ring(int x, int y, int radius, const std::tuple<int,int,int>& color = {255,255,255}, double alpha = 0.6) override;
    void light(int x, int y, int radius, const std::tuple<int,int,int>& color = {255,220,150}, double intensity = 1.0) override;
    void ambient(const std::string& color, double darkness) override;
    void weather(const std::string& kind, double intensity, double t, double flash = 0.0) override;
    void ui_rect(int x, int y, int w, int h, const std::string& color, double alpha = 1.0, const std::string& border = "") override;
    void ui_graphic(const std::string& graphic, const std::string& frame, int x, int y, int scale = 1) override;
    void fade(double alpha, const std::tuple<int,int,int>& color = {0,0,0}) override;
    void flash(double alpha, const std::tuple<int,int,int>& color = {255,255,255}) override;
    void ui_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color = {255,255,255}, bool shadow = true) override;

private:
    QImage graphic_image(const std::string& gid, const std::string& frame);
};

class GameView : public QWidget {
    Q_OBJECT
    Project project;
    Game game;
    QtRenderer renderer;
    QTimer timer;
    InputState input_state;
    bool debugHud = false;

public:
    explicit GameView(const std::string& project_path, QWidget* parent = nullptr);
    void reloadProject();
    void loadProject(const std::string& project_path);
    void setProject(const Project& source);
    void syncProject(const Project& source);
    void openDebugMenu();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
};

} // namespace aur
