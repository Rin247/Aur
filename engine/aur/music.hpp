#pragma once

#include "json.hpp"
#include <string>
#include <vector>

namespace aur {

class MusicEngine {
public:
    std::string current_song;
    double t = 0.0;
    int bpm = 120;
    std::vector<std::string> queue;

    void play(const std::string& song) { current_song = song; t = 0.0; }
    void stop() { current_song.clear(); }
    void update(double dt) { t += dt; }
    bool is_playing() const { return !current_song.empty(); }
};

} // namespace aur