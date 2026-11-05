#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <map>

namespace aur {

inline constexpr int field_tile_size = 32;

struct Renderer {
    int w = 640;
    int h = 360;
    std::string style = "hd2d";
    double zoom = 1.0;

    virtual ~Renderer() = default;

    virtual void begin(const std::string& st = "hd2d") {
        style = st;
        w = (style == "hd2d") ? 640 : 256;
        h = (style == "hd2d") ? 360 : 224;
    }
    virtual void end() {}
    virtual void camera(int x, int y) {}
    virtual void tile(const std::string& tileset, int index, int x, int y, const std::string& layer = "ground") {}
    virtual void sprite(const std::string& graphic, const std::string& frame, int x, int y, bool flip = false, double scale = 1.0, const std::string& tint = "", double alpha = 1.0, bool shadow = true, int z = -1) {}
    virtual void ground_rect(int x, int y, int w, int h, const std::string& color, double alpha = 0.5) {}
    virtual void world_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color = {255,255,255}) {}
    virtual void arc(int x, int y, int radius, double angle, double span_deg, const std::tuple<int,int,int>& color = {255,255,255}, double alpha = 0.5) {}
    virtual void ring(int x, int y, int radius, const std::tuple<int,int,int>& color = {255,255,255}, double alpha = 0.6) {}
    virtual void light(int x, int y, int radius, const std::tuple<int,int,int>& color = {255,220,150}, double intensity = 1.0) {}
    virtual void ambient(const std::string& color, double darkness) {}
    virtual void weather(const std::string& kind, double intensity, double t, double flash = 0.0) {}
    virtual void ui_rect(int x, int y, int w, int h, const std::string& color, double alpha = 1.0, const std::string& border = "") {}
    virtual void ui_text(int x, int y, const std::string& text, const std::tuple<int,int,int>& color = {255,255,255}, bool shadow = true) {}
    virtual void ui_graphic(const std::string& graphic, const std::string& frame, int x, int y, int scale = 1) {}
    virtual void fade(double alpha, const std::tuple<int,int,int>& color = {0,0,0}) {}
    virtual void flash(double alpha, const std::tuple<int,int,int>& color = {255,255,255}) {}
};

class NullRenderer : public Renderer {
public:
    int frames = 0;
    std::vector<std::string> calls;

    void begin(const std::string& st = "snes") override {
        Renderer::begin(st);
        calls.clear();
    }

    void end() override {
        frames++;
    }

    void camera(int x, int y) override { calls.push_back("camera"); }
    void tile(const std::string& t, int i, int x, int y, const std::string& l) override { calls.push_back("tile"); }
    void sprite(const std::string& g, const std::string& f, int x, int y, bool fl, double sc, const std::string& tn, double al, bool sh, int z) override { calls.push_back("sprite"); }
    void ground_rect(int x, int y, int w, int h, const std::string& c, double a) override { calls.push_back("ground_rect"); }
    void world_text(int x, int y, const std::string& txt, const std::tuple<int,int,int>& c) override { calls.push_back("world_text"); }
    void arc(int x, int y, int r, double a, double s, const std::tuple<int,int,int>& c, double al) override { calls.push_back("arc"); }
    void ring(int x, int y, int r, const std::tuple<int,int,int>& c, double al) override { calls.push_back("ring"); }
    void light(int x, int y, int r, const std::tuple<int,int,int>& c, double i) override { calls.push_back("light"); }
    void ambient(const std::string& c, double d) override { calls.push_back("ambient"); }
    void weather(const std::string& k, double i, double t, double f) override { calls.push_back("weather"); }
    void ui_rect(int x, int y, int w, int h, const std::string& c, double a, const std::string& b) override { calls.push_back("ui_rect"); }
    void ui_text(int x, int y, const std::string& txt, const std::tuple<int,int,int>& c, bool s) override {
        calls.push_back("ui_text");
        if (!txt.empty()) text_list.push_back(txt);
    }
    void ui_graphic(const std::string& g, const std::string& f, int x, int y, int sc) override { calls.push_back("ui_graphic"); }
    void fade(double a, const std::tuple<int,int,int>& c) override { calls.push_back("fade"); }
    void flash(double a, const std::tuple<int,int,int>& c) override { calls.push_back("flash"); }

    std::vector<std::string> text_list;
    int count(const std::string& name) const {
        int cnt = 0;
        for (const auto& c : calls) if (c == name) cnt++;
        return cnt;
    }
};

} // namespace aur
