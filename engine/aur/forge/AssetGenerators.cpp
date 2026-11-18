#include "aur/forge.hpp"
#include "aur/project.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace aur {
namespace {

std::string shade(const std::string& color, double amount) {
    const Rgb source = hex_to_rgba(color);
    const auto channel = [amount](int value) {
        return std::clamp(static_cast<int>(value * amount), 0, 255);
    };
    return rgb_to_hex({channel(source.r), channel(source.g), channel(source.b), 255});
}

std::string lighten(const std::string& color, double amount) {
    const Rgb source = hex_to_rgba(color);
    const auto channel = [amount](int value) {
        return std::clamp(static_cast<int>(value + (255 - value) * amount), 0, 255);
    };
    return rgb_to_hex({channel(source.r), channel(source.g), channel(source.b), 255});
}

Json rows_json(const std::vector<std::vector<int>>& grid) {
    JsonArray rows;
    constexpr char digits[] = "0123456789ABCDEF";
    for (const auto& row : grid) {
        std::string encoded;
        encoded.reserve(row.size());
        for (int pixel : row) encoded.push_back(digits[std::clamp(pixel, 0, 15)]);
        rows.emplace_back(std::move(encoded));
    }
    return Json(rows);
}

void rect(std::vector<std::vector<int>>& grid, int x, int y, int width, int height, int color) {
    const int h = static_cast<int>(grid.size());
    const int w = h > 0 ? static_cast<int>(grid[0].size()) : 0;
    for (int py = std::max(0, y); py < std::min(h, y + height); ++py) {
        for (int px = std::max(0, x); px < std::min(w, x + width); ++px) {
            grid[static_cast<size_t>(py)][static_cast<size_t>(px)] = color;
        }
    }
}

void pixel(std::vector<std::vector<int>>& grid, int x, int y, int color) {
    if (y >= 0 && y < static_cast<int>(grid.size()) && x >= 0 &&
        x < static_cast<int>(grid[static_cast<size_t>(y)].size())) {
        grid[static_cast<size_t>(y)][static_cast<size_t>(x)] = color;
    }
}

void ellipse(std::vector<std::vector<int>>& grid, int cx, int cy, int rx, int ry, int color) {
    if (rx <= 0 || ry <= 0) return;
    for (int y = -ry; y <= ry; ++y) {
        const double row = 1.0 - static_cast<double>(y * y) / (ry * ry);
        const int half_width = static_cast<int>(std::floor(rx * std::sqrt(std::max(0.0, row))));
        for (int x = -half_width; x <= half_width; ++x) pixel(grid, cx + x, cy + y, color);
    }
}

void line(std::vector<std::vector<int>>& grid, int x0, int y0, int x1, int y1, int color) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        pixel(grid, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        const int doubled_error = 2 * error;
        if (doubled_error >= dy) { error += dy; x0 += sx; }
        if (doubled_error <= dx) { error += dx; y0 += sy; }
    }
}

} // namespace

Json generate_creature_graphic(const std::string& species, int seed) {
    const std::vector<std::string> supported = {"cat", "dog", "slime", "wolf", "bat", "golem"};
    if (std::find(supported.begin(), supported.end(), species) == supported.end()) {
        throw std::runtime_error("Creature species must be cat, dog, slime, wolf, bat, or golem.");
    }
    std::mt19937 rng(static_cast<uint32_t>(seed));
    const std::string body = species == "slime" ? "#68b989" :
                             species == "golem" ? "#9b8c7b" :
                             species == "bat" ? "#7866a5" :
                             species == "cat" ? "#c98a54" : "#b47b50";
    Json graphic = new_graphic(24, 24, {
        "#000000", "#30283a", lighten(body, 0.28), body, shade(body, 0.67),
        "#f2d6ae", "#473442", "#d65761", "#c5d5d8", "#f0c05e",
        "#719a62", "#7d659c", "#e8e4d8", "#553c32", "#ee9b56", "#f7e9c7"
    }, {12, 21});
    JsonObject frames;
    for (int frame = 0; frame < 4; ++frame) {
        std::vector<std::vector<int>> grid(24, std::vector<int>(24, 0));
        if (species == "bat") {
            rect(grid, 5, 8, 14, 9, 1);
            rect(grid, 7, 9, 10, 7, 3);
            rect(grid, frame % 2 == 0 ? 1 : 3, 7, 7, 7, 4);
            rect(grid, frame % 2 == 0 ? 16 : 14, 7, 7, 7, 4);
            rect(grid, 8, 11, 2, 2, 15);
            rect(grid, 14, 11, 2, 2, 15);
            pixel(grid, 9, 12, 7);
            pixel(grid, 15, 12, 7);
        } else if (species == "golem") {
            rect(grid, 5, 5, 14, 16, 1);
            rect(grid, 6, 6, 12, 14, 3);
            rect(grid, 3, 9, 3, 7, 4);
            rect(grid, 18, 9, 3, 7, 4);
            rect(grid, 8, 9, 3, 3, 9);
            rect(grid, 14, 9, 3, 3, 9);
            rect(grid, 9, 15, 6, 2, 4);
            pixel(grid, 11, 13, 8);
            pixel(grid, 12, 13, 8);
        } else {
            const int bob = frame % 2;
            rect(grid, 4, 10 + bob, 16, 9 - bob, 1);
            rect(grid, 5, 10 + bob, 14, 7, 3);
            if (species == "slime") {
                rect(grid, 7, 12 + bob, 3, 2, 15);
                rect(grid, 14, 12 + bob, 3, 2, 15);
                pixel(grid, 8, 13 + bob, 7);
                pixel(grid, 15, 13 + bob, 7);
            } else {
                rect(grid, 6, 7 + bob, 4, 5, 1);
                rect(grid, 14, 7 + bob, 4, 5, 1);
                rect(grid, 7, 8 + bob, 2, 3, 3);
                rect(grid, 15, 8 + bob, 2, 3, 3);
                if (species == "cat") {
                    rect(grid, 19, 13, 3, 2, 3);
                    pixel(grid, 10, 13 + bob, 7);
                    pixel(grid, 15, 13 + bob, 7);
                } else {
                    rect(grid, 19, 14, 3, 2, 3);
                    rect(grid, 10, 13 + bob, 2, 2, 15);
                    rect(grid, 14, 13 + bob, 2, 2, 15);
                    pixel(grid, 11, 14 + bob, 7);
                    pixel(grid, 14, 14 + bob, 7);
                }
            }
            const int leg_offset = frame % 2;
            rect(grid, 6, 18, 3, 4 - leg_offset, 4);
            rect(grid, 15, 18, 3, 4 - (1 - leg_offset), 4);
            pixel(grid, 8, 11 + bob, 10);
            pixel(grid, 15, 11 + bob, 10);
        }
        if ((rng() % 3) == 0 && species != "golem") {
            pixel(grid, 4 + static_cast<int>(rng() % 16), 8 + static_cast<int>(rng() % 9), 2);
        }
        frames["idle_" + std::to_string(frame)] = rows_json(grid);
    }
    graphic["frames"] = frames;
    graphic["anims"] = JsonObject{
        {"idle", JsonObject{{"frames", JsonArray{"idle_0", "idle_1", "idle_2", "idle_3"}}, {"fps", 4}}},
        {"attack", JsonObject{{"frames", JsonArray{"idle_1", "idle_2", "idle_3"}}, {"fps", 10}}}
    };
    return graphic;
}

Json generate_effect_graphic(const std::string& effect_kind, int seed) {
    const std::vector<std::string> supported = {
        "spark", "fire", "water", "slash", "impact", "heal", "magic", "dust", "explosion"
    };
    if (std::find(supported.begin(), supported.end(), effect_kind) == supported.end()) {
        throw std::runtime_error("Unsupported effect kind.");
    }
    std::mt19937 rng(static_cast<uint32_t>(seed));
    const std::vector<std::string> colors =
        effect_kind == "fire" || effect_kind == "explosion"
            ? std::vector<std::string>{"#241b32", "#63364d", "#b64c49", "#e57c45", "#f6c95d", "#fff1bd"}
            : effect_kind == "water"
                ? std::vector<std::string>{"#202c48", "#315f88", "#4b9fc0", "#a7e4dc", "#f3fbec", "#d67a88"}
                : effect_kind == "heal" || effect_kind == "magic"
                    ? std::vector<std::string>{"#2c2850", "#5b50a0", "#8f80d3", "#8bd6bb", "#e1f6c8", "#f6e8ac"}
                    : std::vector<std::string>{"#29263b", "#59566d", "#8995a8", "#d2d7d4", "#fff2c7", "#df8293"};
    Json graphic = new_graphic(32, 32, {
        "#000000", "#29263b", colors[0], colors[1], colors[2], colors[3], colors[4], colors[5],
        "#f7f1df", "#e1d8b8", "#cabd9c", "#b0a58d", "#8b8290", "#706d88", "#51536f", "#343650"
    }, {16, 16});
    JsonObject frames;
    constexpr double center = 15.5;
    for (int frame = 0; frame < 6; ++frame) {
        std::vector<std::vector<int>> grid(32, std::vector<int>(32, 0));
        const double radius = 3.0 + frame * 2.0;
        const double angle = frame * 0.37 + static_cast<double>(rng() % 100) / 100.0;
        for (int y = 0; y < 32; ++y) {
            for (int x = 0; x < 32; ++x) {
                const double dx = x - center;
                const double dy = y - center;
                const double distance = std::hypot(dx, dy);
                const double theta = std::atan2(dy, dx) - angle;
                if (effect_kind == "slash") {
                    if (std::abs(std::sin(theta)) < 0.11 && distance > radius * 0.45 &&
                        distance < radius + 7.0) grid[static_cast<size_t>(y)][static_cast<size_t>(x)] = 6;
                } else if (effect_kind == "impact" || effect_kind == "spark") {
                    const int spokes = effect_kind == "impact" ? 8 : 6;
                    const double spoke = std::abs(std::sin(theta * spokes));
                    if (distance > radius * 0.45 && distance < radius + 2.0 && spoke < 0.18) {
                        grid[static_cast<size_t>(y)][static_cast<size_t>(x)] = 3 + frame % 4;
                    }
                } else {
                    const double wobble = std::sin(x * 0.72 + frame) + std::cos(y * 0.51 - frame);
                    const double wave = radius + wobble * 1.6;
                    if (distance < wave && distance > wave * 0.42) {
                        grid[static_cast<size_t>(y)][static_cast<size_t>(x)] =
                            std::clamp(7 - static_cast<int>((distance / std::max(1.0, wave)) * 4.0), 2, 7);
                    }
                }
            }
        }
        const int particle_count = 5 + frame;
        for (int particle = 0; particle < particle_count; ++particle) {
            const double a = angle + particle * 6.28318530718 / particle_count;
            const int orbit = 4 + frame * 2;
            const int x = static_cast<int>(center + std::cos(a) * orbit);
            const int y = static_cast<int>(center + std::sin(a) * orbit);
            pixel(grid, x, y, 7);
            if ((rng() % 2) == 0) pixel(grid, x + 1, y, 5);
        }
        if (effect_kind == "heal" || effect_kind == "magic") {
            rect(grid, 15, 11, 3, 10, 7);
            rect(grid, 11, 15, 10, 3, 7);
        }
        frames[std::to_string(frame)] = rows_json(grid);
    }
    graphic["frames"] = frames;
    graphic["anims"] = JsonObject{
        {"play", JsonObject{{"frames", JsonArray{"0", "1", "2", "3", "4", "5"}}, {"fps", 12}, {"loop", false}}}
    };
    graphic["effect_kind"] = effect_kind;
    return graphic;
}

Json generate_clothing_graphic(const std::string& kind, int seed) {
    const std::vector<std::string> supported = {
        "shirt", "tunic", "pants", "shorts", "skirt", "dress", "cloak", "boots", "hat"
    };
    if (std::find(supported.begin(), supported.end(), kind) == supported.end()) {
        throw std::runtime_error("Unsupported clothing kind.");
    }

    std::mt19937 rng(static_cast<uint32_t>(seed));
    const std::vector<std::string> cloth_colors = {
        "#3b6ea5", "#a74640", "#7957a0", "#3e8061", "#b58b3b", "#3f8790"
    };
    const std::string cloth = cloth_colors[static_cast<size_t>(rng() % cloth_colors.size())];
    const std::string trim = (rng() % 2 == 0) ? "#e5bd68" : "#dfd5ba";
    Json graphic = new_graphic(32, 32, {
        "#000000", "#292536", cloth, shade(cloth, 0.62), lighten(cloth, 0.28), trim,
        "#62463f", "#3e3037", "#737d8c", "#c4cbd0", "#f3dfb2", "#a97846",
        "#81563f", "#493844", "#b64c49", "#f3eee0"
    }, {16, 28});
    std::vector<std::vector<int>> grid(32, std::vector<int>(32, 0));

    const auto tapered = [&grid](int top, int bottom, int top_left, int top_right,
                                 int bottom_left, int bottom_right, int color) {
        const int rows = std::max(1, bottom - top);
        for (int y = top; y < bottom; ++y) {
            const double t = static_cast<double>(y - top) / rows;
            const int left = static_cast<int>(top_left + (bottom_left - top_left) * t);
            const int right = static_cast<int>(top_right + (bottom_right - top_right) * t);
            rect(grid, left, y, right - left + 1, 1, color);
        }
    };

    if (kind == "shirt" || kind == "tunic" || kind == "cloak") {
        const int hem = kind == "shirt" ? 22 : kind == "tunic" ? 27 : 29;
        tapered(7, 12, 10, 21, 5, 26, 1);
        tapered(8, 12, 11, 20, 6, 25, 2);
        rect(grid, 4, 10, 6, 8, 1);
        rect(grid, 22, 10, 6, 8, 1);
        rect(grid, 5, 11, 4, 6, 2);
        rect(grid, 23, 11, 4, 6, 2);
        tapered(10, hem + 1, 8, 23, kind == "cloak" ? 2 : 7,
                kind == "cloak" ? 29 : 24, 1);
        tapered(11, hem, 9, 22, kind == "cloak" ? 3 : 8,
                kind == "cloak" ? 28 : 23, 2);
        rect(grid, 13, 9, 6, 2, 1);
        rect(grid, 14, 9, 4, 2, 0);
        line(grid, 10, 13, kind == "cloak" ? 5 : 10, hem - 3, 3);
        line(grid, 21, 13, kind == "cloak" ? 26 : 21, hem - 3, 4);
        rect(grid, kind == "cloak" ? 3 : 8, hem - 2,
             kind == "cloak" ? 26 : 16, 2, 5);
        if (kind == "tunic") {
            rect(grid, 15, 13, 2, 8, 5);
            rect(grid, 13, 21, 6, 2, 5);
        } else if (kind == "cloak") {
            rect(grid, 14, 12, 4, 4, 5);
        } else {
            rect(grid, 15, 13, 2, 6, 5);
        }
        pixel(grid, 15, hem - 4, 10);
        pixel(grid, 16, hem - 4, 10);
    } else if (kind == "pants" || kind == "shorts") {
        const int leg_bottom = kind == "pants" ? 28 : 20;
        rect(grid, 8, 6, 16, 5, 1);
        rect(grid, 9, 7, 14, 3, 2);
        rect(grid, 8, 10, 7, leg_bottom - 9, 1);
        rect(grid, 17, 10, 7, leg_bottom - 9, 1);
        rect(grid, 9, 11, 5, leg_bottom - 11, 2);
        rect(grid, 18, 11, 5, leg_bottom - 11, 2);
        rect(grid, 15, 11, 2, leg_bottom - 11, 1);
        rect(grid, 10, 8, 12, 1, 5);
        if (kind == "pants") {
            rect(grid, 8, 26, 7, 3, 1);
            rect(grid, 17, 26, 7, 3, 1);
            rect(grid, 9, 27, 5, 1, 5);
            rect(grid, 18, 27, 5, 1, 5);
            line(grid, 11, 13, 11, 23, 3);
            line(grid, 20, 13, 20, 23, 4);
        } else {
            rect(grid, 8, 18, 7, 3, 1);
            rect(grid, 17, 18, 7, 3, 1);
            rect(grid, 9, 18, 5, 1, 5);
            rect(grid, 18, 18, 5, 1, 5);
        }
        rect(grid, 14, 11, 4, 3, 5);
        pixel(grid, 16, 12, 10);
    } else if (kind == "skirt" || kind == "dress") {
        if (kind == "dress") {
            tapered(6, 13, 11, 20, 8, 23, 1);
            tapered(7, 13, 12, 19, 9, 22, 2);
            rect(grid, 5, 9, 5, 7, 1);
            rect(grid, 22, 9, 5, 7, 1);
            rect(grid, 6, 10, 3, 5, 2);
            rect(grid, 23, 10, 3, 5, 2);
            rect(grid, 13, 6, 6, 2, 1);
            rect(grid, 14, 7, 4, 1, 0);
            rect(grid, 10, 12, 12, 3, 5);
            pixel(grid, 16, 13, 10);
        } else {
            rect(grid, 9, 6, 14, 5, 1);
            rect(grid, 10, 7, 12, 3, 2);
        }
        tapered(kind == "dress" ? 14 : 10, 28, 10, 21, 4, 27, 1);
        tapered(kind == "dress" ? 15 : 11, 26, 11, 20, 5, 26, 2);
        for (int x : {8, 12, 16, 20, 24}) pixel(grid, x, 20 + (x % 3), 4);
        rect(grid, 5, 26, 22, 2, 5);
        line(grid, 11, 15, 8, 24, 3);
        line(grid, 20, 15, 23, 24, 4);
        if (kind == "dress") rect(grid, 13, 15, 6, 2, 5);
    } else if (kind == "boots") {
        rect(grid, 6, 7, 8, 17, 1);
        rect(grid, 18, 7, 8, 17, 1);
        rect(grid, 7, 8, 6, 13, 6);
        rect(grid, 19, 8, 6, 13, 6);
        rect(grid, 5, 22, 10, 6, 1);
        rect(grid, 17, 22, 10, 6, 1);
        rect(grid, 6, 23, 8, 3, 6);
        rect(grid, 18, 23, 8, 3, 6);
        rect(grid, 8, 11, 2, 8, 5);
        rect(grid, 20, 11, 2, 8, 5);
        rect(grid, 7, 26, 7, 1, 10);
        rect(grid, 19, 26, 7, 1, 10);
    } else {
        tapered(7, 14, 8, 23, 4, 27, 1);
        tapered(8, 13, 9, 22, 5, 26, 2);
        rect(grid, 10, 7, 12, 2, 5);
        rect(grid, 14, 8, 4, 2, 10);
        line(grid, 7, 12, 24, 12, 3);
    }

    graphic["frames"]["0"] = rows_json(grid);
    graphic["clothing_kind"] = kind;
    graphic["seed"] = seed;
    return graphic;
}

Json generate_prop_graphic(const std::string& kind, int seed) {
    const std::vector<std::string> supported = {
        "sword", "staff", "shield", "bow", "lantern", "satchel", "potion"
    };
    if (std::find(supported.begin(), supported.end(), kind) == supported.end()) {
        throw std::runtime_error("Unsupported prop kind.");
    }

    std::mt19937 rng(static_cast<uint32_t>(seed));
    const std::string accent = (rng() % 2 == 0) ? "#c94d49" : "#629a83";
    Json graphic = new_graphic(32, 32, {
        "#000000", "#292536", "#62463f", "#3e3037", "#c4cbd0", "#f0d47a",
        "#8c98a5", "#dce3e5", "#5a8b72", accent, "#a97846", "#81563f",
        "#db6a54", "#74b9bd", "#f3eee0", "#475b84"
    }, {16, 28});
    std::vector<std::vector<int>> grid(32, std::vector<int>(32, 0));
    const auto tapered = [&grid](int top, int bottom, int top_left, int top_right,
                                 int bottom_left, int bottom_right, int color) {
        const int rows = std::max(1, bottom - top);
        for (int y = top; y < bottom; ++y) {
            const double t = static_cast<double>(y - top) / rows;
            const int left = static_cast<int>(top_left + (bottom_left - top_left) * t);
            const int right = static_cast<int>(top_right + (bottom_right - top_right) * t);
            rect(grid, left, y, right - left + 1, 1, color);
        }
    };

    if (kind == "sword") {
        line(grid, 10, 23, 23, 8, 1);
        line(grid, 11, 22, 22, 9, 7);
        line(grid, 12, 22, 21, 11, 4);
        line(grid, 8, 20, 15, 27, 5);
        line(grid, 9, 20, 14, 25, 10);
        rect(grid, 9, 25, 5, 2, 1);
        pixel(grid, 9, 26, 5);
    } else if (kind == "staff") {
        line(grid, 15, 26, 17, 10, 1);
        line(grid, 16, 25, 17, 11, 10);
        ellipse(grid, 16, 7, 5, 5, 1);
        ellipse(grid, 16, 7, 3, 3, 9);
        pixel(grid, 15, 6, 14);
        pixel(grid, 17, 8, 13);
    } else if (kind == "shield") {
        tapered(5, 22, 8, 23, 11, 20, 1);
        tapered(6, 20, 9, 22, 12, 19, 15);
        tapered(7, 19, 10, 21, 13, 18, 6);
        rect(grid, 14, 8, 4, 10, 5);
        rect(grid, 11, 11, 10, 3, 5);
        pixel(grid, 16, 12, 10);
    } else if (kind == "bow") {
        line(grid, 12, 5, 20, 9, 5);
        line(grid, 20, 9, 22, 16, 5);
        line(grid, 22, 16, 20, 23, 5);
        line(grid, 20, 23, 12, 27, 5);
        line(grid, 12, 5, 12, 27, 4);
        line(grid, 12, 16, 23, 16, 1);
        pixel(grid, 22, 15, 10);
    } else if (kind == "lantern") {
        rect(grid, 10, 11, 12, 15, 1);
        rect(grid, 12, 13, 8, 11, 5);
        rect(grid, 13, 14, 6, 8, 10);
        rect(grid, 11, 8, 10, 3, 4);
        rect(grid, 13, 6, 6, 2, 1);
        line(grid, 13, 5, 17, 3, 1);
        line(grid, 17, 3, 21, 5, 1);
        pixel(grid, 16, 15, 14);
        pixel(grid, 17, 18, 14);
    } else if (kind == "satchel") {
        line(grid, 10, 7, 20, 7, 1);
        rect(grid, 8, 10, 16, 16, 1);
        rect(grid, 9, 11, 14, 13, 10);
        rect(grid, 11, 15, 10, 8, 11);
        rect(grid, 10, 13, 12, 3, 5);
        rect(grid, 14, 15, 4, 2, 1);
        pixel(grid, 16, 16, 5);
        line(grid, 10, 12, 12, 10, 5);
    } else {
        ellipse(grid, 16, 18, 8, 10, 1);
        ellipse(grid, 16, 18, 6, 8, 12);
        rect(grid, 12, 6, 8, 4, 1);
        rect(grid, 13, 7, 6, 2, 4);
        rect(grid, 12, 11, 8, 2, 7);
        rect(grid, 12, 16, 8, 2, 14);
        pixel(grid, 13, 14, 13);
        pixel(grid, 18, 21, 9);
    }

    graphic["frames"]["0"] = rows_json(grid);
    graphic["prop_kind"] = kind;
    graphic["seed"] = seed;
    return graphic;
}

Json generate_tile_asset(const std::string& kind, int seed, int width, int height,
                        int base, int dark, int light, int accent) {
    const std::vector<std::string> supported = {
        "grass", "sand", "dirt", "stone", "brick", "planks", "water", "snow",
        "lava", "crystal", "foliage", "noise", "dunes", "marsh", "ruins", "sky"
    };
    if (std::find(supported.begin(), supported.end(), kind) == supported.end()) {
        throw std::runtime_error("Unsupported tile texture kind.");
    }
    if (width < 8 || width > 64 || height < 8 || height > 64 ||
        base < 0 || base > 15 || dark < 0 || dark > 15 || light < 0 || light > 15 ||
        accent < 0 || accent > 15) {
        throw std::runtime_error("Tile dimensions must be 8–64 and palette indexes 0–15.");
    }
    std::mt19937 rng(static_cast<uint32_t>(seed));
    std::vector<std::vector<int>> grid(static_cast<size_t>(height),
                                       std::vector<int>(static_cast<size_t>(width), base));
    if (kind == "stone" || kind == "brick") {
        const int row_height = kind == "stone" ? 5 : 4;
        const int block_width = kind == "stone" ? 8 : 8;
        for (int y = 0; y < height; y += row_height) {
            for (int x = 0; x < width; ++x) pixel(grid, x, y, dark);
            const int offset = static_cast<int>(rng() % static_cast<uint32_t>(block_width));
            const int start = (y / row_height % 2) * (block_width / 2) + offset;
            for (int x = start; x < width; x += block_width) {
                for (int py = y; py < std::min(height, y + row_height); ++py) pixel(grid, x, py, dark);
            }
        }
    } else if (kind == "planks") {
        for (int y = 3; y < height; y += 4) {
            for (int x = 0; x < width; ++x) pixel(grid, x, y, dark);
        }
        for (int y = 0; y < height; y += 4) {
            pixel(grid, static_cast<int>(rng() % static_cast<uint32_t>(width)), y + 1, accent);
        }
    } else if (kind == "water") {
        for (int y = 2; y < height; y += 6) {
            const int offset = static_cast<int>(rng() % static_cast<uint32_t>(width));
            for (int x = 0; x < width; x += 8) {
                for (int k = 0; k < 3; ++k) pixel(grid, x + offset + k, y, light);
            }
        }
    } else if (kind == "snow") {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const unsigned value = rng() % 32;
                if (value == 0) pixel(grid, x, y, dark);
                else if (value < 4) pixel(grid, x, y, light);
            }
        }
    } else if (kind == "lava") {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const double wave = std::sin((x + y * 0.55) * 0.42);
                if (wave > 0.15 && rng() % 3 != 0) pixel(grid, x, y, accent);
                else if (rng() % 11 == 0) pixel(grid, x, y, light);
            }
        }
    } else if (kind == "crystal") {
        for (int x = 1; x < width; x += 5) {
            const int top = static_cast<int>(rng() % static_cast<uint32_t>(std::max(1, height / 2)));
            const int shard_height = std::max(2, height / 3 + static_cast<int>(rng() % static_cast<uint32_t>(std::max(1, height / 4))));
            for (int y = top; y < std::min(height, top + shard_height); ++y) {
                pixel(grid, x, y, light);
                if (x + 1 < width && y > top) pixel(grid, x + 1, y, accent);
            }
        }
    } else if (kind == "dunes") {
        for (int y = 0; y < height; ++y) {
            const double curve = std::sin(y * 0.19 + seed * 0.01) * width * 0.16;
            const int crest = static_cast<int>(width * 0.48 + curve);
            for (int x = 0; x < width; ++x) {
                const int distance = std::abs(x - crest);
                if (distance < 2) pixel(grid, x, y, light);
                else if (distance < 5 && (y + seed) % 3 != 0) pixel(grid, x, y, accent);
                else if ((x * 7 + y * 11 + seed) % 67 == 0) pixel(grid, x, y, dark);
            }
        }
    } else if (kind == "marsh") {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if ((x * 13 + y * 7 + seed) % 47 < 4) pixel(grid, x, y, dark);
                if ((x * 5 + y * 11 + seed) % 59 < 3) pixel(grid, x, y, light);
            }
        }
        for (int x = 2; x < width; x += std::max(4, width / 6)) {
            const int reed_height = height / 4 + static_cast<int>(rng() %
                static_cast<uint32_t>(std::max(1, height / 4)));
            const int reed_x = std::min(width - 1, x + static_cast<int>(rng() % 3));
            const int reed_y = std::max(1, height - reed_height);
            line(grid, reed_x, height - 1, reed_x, reed_y, accent);
            pixel(grid, reed_x - 1, reed_y, light);
            if (reed_x + 1 < width) pixel(grid, reed_x + 1, reed_y + 1, light);
        }
    } else if (kind == "ruins") {
        const int block_height = std::max(4, height / 6);
        const int block_width = std::max(6, width / 4);
        for (int y = 0; y < height; y += block_height) {
            const int offset = (y / block_height % 2) * (block_width / 2);
            for (int x = -offset; x < width; x += block_width) {
                if (rng() % 7 == 0) continue;
                const int left = std::max(0, x + 1);
                const int top = y + 1 + static_cast<int>(rng() % 2);
                const int right = std::min(width, x + block_width - 1);
                const int bottom = std::min(height, y + block_height - 1);
                rect(grid, left, top, right - left, bottom - top, light);
                if (rng() % 3 == 0) {
                    pixel(grid, left + static_cast<int>(rng() %
                        static_cast<uint32_t>(std::max(1, right - left))),
                        std::min(height - 1, bottom), accent);
                }
            }
        }
    } else if (kind == "sky") {
        const int soil_line = std::max(2, height / 3);
        for (int y = soil_line; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int strata = (y + seed) % 7;
                pixel(grid, x, y, strata < 2 ? dark : base);
                if ((x * 3 + y * 5 + seed) % 31 == 0) pixel(grid, x, y, accent);
            }
        }
        for (int x = 0; x < width; ++x) {
            if ((x + seed) % 5 == 0) pixel(grid, x, soil_line - 1, dark);
            if ((x * 11 + seed) % 7 < 5) pixel(grid, x, soil_line, light);
        }
        for (int x = 2; x < width; x += std::max(5, width / 5)) {
            if (rng() % 2 == 0) continue;
            const int cloud_y = 2 + static_cast<int>(rng() %
                static_cast<uint32_t>(std::max(1, soil_line - 2)));
            rect(grid, x, cloud_y, std::min(3, width - x), 1, light);
        }
    } else {
        const int dots = width * height / (kind == "noise" ? 5 : 18);
        for (int i = 0; i < dots; ++i) {
            const int x = static_cast<int>(rng() % static_cast<uint32_t>(width));
            const int y = static_cast<int>(rng() % static_cast<uint32_t>(height));
            int color = dark;
            if (kind == "foliage" && rng() % 3 == 0) color = accent;
            else if (rng() % 3 == 0) color = light;
            pixel(grid, x, y, color);
            if (kind == "grass" && y > 0 && rng() % 3 == 0) pixel(grid, x, y - 1, accent);
        }
    }

    Json tile;
    tile["name"] = kind + "_" + std::to_string(seed);
    tile["rows"] = rows_json(grid);
    tile["flags"] = kind == "water" ? WATER : kind == "lava" ? HURT : 0;
    tile["pal"] = 0;
    return tile;
}

JsonArray generate_starter_world_terrain_tiles(int palette_count) {
    struct StarterTile {
        const char* name;
        const char* kind;
        int seed;
        int palette;
        int base;
        int dark;
        int light;
        int accent;
    };
    constexpr std::array<StarterTile, 21> starter_tiles{{
        {"starter_grassland", "grass", 73101, 0, 3, 2, 4, 3},
        {"starter_sand_coast", "sand", 73102, 2, 3, 2, 4, 12},
        {"starter_rocky_coast", "stone", 73103, 2, 3, 2, 4, 12},
        {"starter_shallow_sea", "water", 73104, 1, 6, 5, 10, 8},
        {"starter_deep_sea", "water", 73105, 1, 11, 14, 2, 3},
        {"starter_rock", "stone", 73106, 0, 10, 9, 11, 12},
        {"starter_scree", "stone", 73107, 3, 6, 5, 12, 10},
        {"starter_snowfield", "snow", 73108, 3, 9, 5, 4, 10},
        {"starter_forest_floor", "foliage", 73109, 0, 2, 1, 3, 4},
        {"biome_verdant_plains", "grass", 73201, 0, 3, 2, 4, 8},
        {"biome_old_growth_forest", "foliage", 73202, 0, 6, 1, 8, 4},
        {"biome_tropical_jungle", "foliage", 73203, 0, 2, 1, 4, 8},
        {"biome_desert_wastes", "dunes", 73204, 2, 3, 2, 4, 12},
        {"biome_alpine_highlands", "stone", 73205, 3, 6, 5, 12, 8},
        {"biome_frostfall_tundra", "snow", 73206, 3, 9, 5, 4, 10},
        {"biome_mistfen_marsh", "marsh", 73207, 0, 6, 1, 8, 4},
        {"biome_volcanic_badlands", "lava", 73208, 2, 13, 1, 9, 10},
        {"biome_coast_and_islands", "sand", 73209, 1, 6, 2, 10, 8},
        {"biome_ancient_ruins", "ruins", 73210, 2, 2, 1, 12, 3},
        {"biome_crystal_reaches", "crystal", 73211, 2, 13, 8, 11, 12},
        {"biome_sky_isles", "sky", 73212, 3, 2, 11, 9, 6}
    }};
    JsonArray tiles;
    for (const StarterTile& starter : starter_tiles) {
        Json tile = generate_tile_asset(starter.kind, starter.seed, 32, 32,
                                        starter.base, starter.dark,
                                        starter.light, starter.accent);
        tile["name"] = starter.name;
        tile["pal"] = palette_count > 0
            ? std::min(starter.palette, palette_count - 1) : 0;
        tiles.push_back(std::move(tile));
    }
    return tiles;
}

} // namespace aur

