#include "aur/forge.hpp"
#include "aur/project.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace aur {
namespace {

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
    if (w == 48 && h == 64) {
        const int x0 = x * 3 / 2;
        const int x1 = (x + width) * 3 / 2;
        const int y0 = y * 4 / 3;
        const int y1 = (y + height) * 4 / 3;
        for (int py = std::max(0, y0); py < std::min(h, y1); ++py) {
            for (int px = std::max(0, x0); px < std::min(w, x1); ++px) {
                grid[static_cast<size_t>(py)][static_cast<size_t>(px)] = color;
            }
        }
        return;
    }
    for (int py = std::max(0, y); py < std::min(h, y + height); ++py) {
        for (int px = std::max(0, x); px < std::min(w, x + width); ++px) {
            grid[static_cast<size_t>(py)][static_cast<size_t>(px)] = color;
        }
    }
}

void pixel(std::vector<std::vector<int>>& grid, int x, int y, int color) {
    if (grid.size() == 64 && !grid.empty() && grid[0].size() == 48) {
        const int x0 = x * 3 / 2;
        const int x1 = (x + 1) * 3 / 2;
        const int y0 = y * 4 / 3;
        const int y1 = (y + 1) * 4 / 3;
        for (int py = y0; py < y1; ++py) {
            for (int px = x0; px < x1; ++px) {
                if (py >= 0 && py < 64 && px >= 0 && px < 48) {
                    grid[static_cast<size_t>(py)][static_cast<size_t>(px)] = color;
                }
            }
        }
        return;
    }
    if (y >= 0 && y < static_cast<int>(grid.size()) && x >= 0 &&
        x < static_cast<int>(grid[static_cast<size_t>(y)].size())) {
        grid[static_cast<size_t>(y)][static_cast<size_t>(x)] = color;
    }
}

void ellipse(std::vector<std::vector<int>>& grid, int cx, int cy, int rx, int ry, int color) {
    if (rx <= 0 || ry <= 0) return;
    if (!grid.empty() && grid.size() == 64 && grid[0].size() == 48) {
        cx = cx * 3 / 2;
        cy = cy * 4 / 3;
        rx = std::max(1, rx * 3 / 2);
        ry = std::max(1, ry * 4 / 3);
        for (int y = -ry; y <= ry; ++y) {
            const double row = 1.0 - static_cast<double>(y * y) / (ry * ry);
            const int half_width = static_cast<int>(std::floor(rx * std::sqrt(std::max(0.0, row))));
            for (int x = -half_width; x <= half_width; ++x) {
                if (cy + y >= 0 && cy + y < 64 && cx + x >= 0 && cx + x < 48) {
                    grid[static_cast<size_t>(cy + y)][static_cast<size_t>(cx + x)] = color;
                }
            }
        }
        return;
    }
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

std::vector<std::vector<int>> mirror_grid(const std::vector<std::vector<int>>& source) {
    auto result = source;
    for (auto& row : result) std::reverse(row.begin(), row.end());
    return result;
}

std::vector<std::vector<int>> character_frame(const Json& spec, const std::string& facing,
                                               int step) {
    std::vector<std::vector<int>> grid(64, std::vector<int>(48, 0));
    const Json& parts = spec["parts"];
    const std::string race = parts.contains("race") && parts["race"].is_string()
        ? parts["race"].as_string() : "kaeli";
    const bool child = parts.contains("age_group") && parts["age_group"].as_string() == "child";
    const bool teen = parts.contains("age_group") && parts["age_group"].as_string() == "teen";
    const bool side = facing == "right";
    const bool three_quarter = facing == "down_right" || facing == "up_right";
    const bool profile = side || three_quarter;
    const bool back = facing == "up" || facing == "up_left" || facing == "up_right";
    const bool feminine = parts.contains("presentation") &&
                          parts["presentation"].as_string() == "feminine";
    const bool elven = race == "sylvani" || race == "solani" ||
                       (parts.contains("ears") && parts["ears"].as_string() == "elf");
    const std::string hair_style = parts.contains("hair_style")
        ? parts["hair_style"].as_string() : "short";
    const std::string bottom = parts.contains("bottom") ? parts["bottom"].as_string() : "pants";
    const int hair = 4;
    const int head_shift = child ? 3 : teen ? 1 : 0;
    const int body_shift = child ? 3 : teen ? 1 : 0;
    const int stride = step == 1 ? -2 : (step == 2 ? 2 : 0);
    const int torso_width = side ? (child ? 8 : 9) :
                            child ? 10 : teen ? 11 : feminine ? 12 : 14;
    const int torso_left = (32 - torso_width) / 2;
    const auto head_y = [head_shift](int y) { return y + head_shift; };
    const auto body_y = [body_shift](int y) { return y + body_shift; };
    const bool skirt = bottom == "skirt" || (feminine && bottom != "pants");

    const int head_cx = 16;
    const int head_cy = head_y(12);
    const int head_rx = child ? 7 : 6;
    const int head_ry = child ? 8 : 7;
    const int hair_length = child ? 7 : teen ? 10 : 13;

    pixel(grid, 10, 46, 1);
    rect(grid, 11, 45, 10, 2, 1);
    rect(grid, 13, 46, 6, 1, 11);

    if (elven) {
        for (int row = 0; row < 6; ++row) {
            const int ear_y = head_cy - 2 + row;
            const int ear_width = std::min(row + 1, 6);
            const int left_x = head_cx - 11 + row;
            const int right_x = head_cx + 11 - row - ear_width + 1;
            rect(grid, left_x, ear_y, ear_width, 1, 1);
            rect(grid, right_x, ear_y, ear_width, 1, 1);
            if (row >= 2) {
                rect(grid, left_x + 1, ear_y, ear_width - 2, 1, 2);
                rect(grid, right_x + 1, ear_y, ear_width - 2, 1, 2);
            }
            if (row == 3) {
                pixel(grid, left_x + 1, ear_y, 15);
                pixel(grid, right_x + ear_width - 2, ear_y, 15);
            }
        }
    } else {
        ellipse(grid, head_cx - head_rx, head_cy, 2, 3, 1);
        ellipse(grid, head_cx + head_rx, head_cy, 2, 3, 1);
        pixel(grid, head_cx - head_rx, head_cy, 2);
        pixel(grid, head_cx + head_rx, head_cy, 3);
    }

    if (back) {
        ellipse(grid, head_cx, head_cy, head_rx + 1, head_ry + 1, 1);
        ellipse(grid, head_cx, head_cy, head_rx, head_ry, hair);
        line(grid, head_cx - 3, head_cy - 4, head_cx - 4, head_cy + 4, 13);
        line(grid, head_cx + 1, head_cy - 4, head_cx + 2, head_cy + 5, 5);
        pixel(grid, head_cx + 3, head_cy + 1, 13);
        rect(grid, head_cx - 5, head_y(18), 4, hair_length, 5);
        rect(grid, head_cx + 2, head_y(18), 4, hair_length, hair);
        pixel(grid, head_cx - 4, head_y(20 + hair_length / 2), 13);
        pixel(grid, head_cx + 3, head_y(20 + hair_length / 2), 5);
    } else {
        const int face_width = side ? 4 : head_rx;
        ellipse(grid, head_cx, head_cy, head_rx + 1, head_ry + 1, 1);
        ellipse(grid, head_cx, head_cy, face_width, head_ry, 2);
        ellipse(grid, head_cx - 2, head_cy - 1, std::max(1, face_width - 3), head_ry - 2, 15);
        ellipse(grid, head_cx + 3, head_cy, 2, head_ry - 3, 3);
        rect(grid, head_cx - head_rx - 1, head_y(10), 2, hair_length, hair);
        rect(grid, head_cx - head_rx, head_y(17), 2, 4, 5);
        rect(grid, head_cx + head_rx, head_y(10), 2, hair_length + 1, hair);
        rect(grid, head_cx + head_rx + 1, head_y(18), 1, 3, 5);
        ellipse(grid, head_cx, head_y(6), head_rx - 1, 4, hair);
        rect(grid, head_cx - 4, head_y(7), 3, 3, 13);
        rect(grid, head_cx + 2, head_y(6), 3, 4, 5);
        pixel(grid, head_cx - 4, head_y(8), 13);
        pixel(grid, head_cx + 4, head_y(7), hair);
        if (side) {
            rect(grid, head_cx + 2, head_y(11), 3, 4, 1);
            rect(grid, head_cx + 3, head_y(12), 1, 2, 12);
            pixel(grid, head_cx + 3, head_y(12), 15);
            pixel(grid, head_cx + 5, head_y(14), 3);
            pixel(grid, head_cx + 2, head_y(16), 10);
        } else if (three_quarter) {
            pixel(grid, head_cx - 3, head_y(12), 1);
            rect(grid, head_cx + 1, head_y(11), 3, 4, 1);
            rect(grid, head_cx + 2, head_y(12), 1, 2, 12);
            pixel(grid, head_cx + 2, head_y(12), 15);
            pixel(grid, head_cx + 4, head_y(14), 3);
            pixel(grid, head_cx + 1, head_y(16), 10);
        } else {
            rect(grid, head_cx - 5, head_y(11), 3, 4, 1);
            rect(grid, head_cx + 2, head_y(11), 3, 4, 1);
            rect(grid, head_cx - 4, head_y(12), 2, 2, 12);
            rect(grid, head_cx + 3, head_y(12), 2, 2, 12);
            pixel(grid, head_cx - 4, head_y(12), 15);
            pixel(grid, head_cx + 3, head_y(12), 15);
            pixel(grid, head_cx, head_y(14), 3);
            pixel(grid, head_cx - 1, head_y(16), 3);
            pixel(grid, head_cx, head_y(16), 3);
            pixel(grid, head_cx + 1, head_y(16), 10);
        }
        pixel(grid, head_cx - 3, head_y(7), 13);
        pixel(grid, head_cx + 2, head_y(6), 13);
        pixel(grid, head_cx + 5, head_y(8), 5);
        rect(grid, head_cx - 3, head_y(9), 2, 1, 10);
        pixel(grid, head_cx, head_y(8), 11);
        if (hair_style == "curly") {
            pixel(grid, head_cx - 6, head_y(7), 13);
            pixel(grid, head_cx + 6, head_y(7), 5);
            pixel(grid, head_cx - 4, head_y(5), hair);
            pixel(grid, head_cx + 3, head_y(5), hair);
        }
    }

    if (race == "beastkin" && !back) {
        line(grid, head_cx - 5, head_y(7), head_cx - 8, head_y(1), 1);
        line(grid, head_cx - 8, head_y(1), head_cx - 2, head_y(5), 1);
        line(grid, head_cx + 5, head_y(7), head_cx + 8, head_y(1), 1);
        line(grid, head_cx + 8, head_y(1), head_cx + 2, head_y(5), 1);
        pixel(grid, head_cx - 7, head_y(3), 4);
        pixel(grid, head_cx + 7, head_y(3), 4);
        if (!side) {
            pixel(grid, head_cx - 1, head_y(17), 1);
            pixel(grid, head_cx, head_y(18), 1);
            pixel(grid, head_cx + 1, head_y(17), 1);
        }
    }

    const int center = 16;
    if (race == "featherfolk") {
        if (profile) {
            ellipse(grid, 10, body_y(29), 3, 7, 1);
            line(grid, 9, body_y(27), 8, body_y(22), 15);
            line(grid, 10, body_y(29), 7, body_y(26), 15);
            line(grid, 10, body_y(31), 7, body_y(30), 15);
        } else {
            ellipse(grid, 7, body_y(29), 3, 7, 1);
            ellipse(grid, 25, body_y(29), 3, 7, 1);
            line(grid, 7, body_y(27), 5, body_y(22), 15);
            line(grid, 7, body_y(29), 4, body_y(26), 15);
            line(grid, 7, body_y(31), 4, body_y(30), 15);
            line(grid, 25, body_y(27), 27, body_y(22), 15);
            line(grid, 25, body_y(29), 28, body_y(26), 15);
            line(grid, 25, body_y(31), 28, body_y(30), 15);
        }
    }
    if (race == "beastkin") {
        line(grid, center + 7, body_y(35), center + 10, body_y(33), 1);
        line(grid, center + 10, body_y(33), center + 10, body_y(29), 1);
        pixel(grid, center + 10, body_y(29), 4);
    }

    const int half_body = std::max(side ? 4 : 5, torso_width / 2);
    const int arm_y = body_y(29);
    ellipse(grid, center - half_body - 1, arm_y, 2, 5, 1);
    ellipse(grid, center - half_body - 1, arm_y, 1, 4, 2);
    ellipse(grid, center + half_body + 1, arm_y, 2, 5, 1);
    ellipse(grid, center + half_body + 1, arm_y, 1, 4, 3);

    pixel(grid, center, body_y(20), 2);
    rect(grid, center - 2, body_y(20), 5, 2, 2);
    const int shoulders = half_body - 1;
    for (int row = 0; row < 13; ++row) {
        int half_width = row < 2 ? shoulders - (1 - row) :
                         row < 9 ? half_body : half_body - (row - 8) / 2;
        half_width = std::max(3, half_width);
        const int y = body_y(22 + row);
        rect(grid, center - half_width - 1, y, half_width * 2 + 2, 1, 1);
        rect(grid, center - half_width, y, half_width * 2, 1, row < 2 ? 7 : 6);
    }
    for (int row = 0; row < 8; ++row) {
        const int y = body_y(24 + row);
        pixel(grid, center - half_body + 1, y, row < 3 ? 15 : 6);
        pixel(grid, center + half_body - 2, y, row < 4 ? 7 : 6);
    }
    line(grid, center - 2, body_y(23), center, body_y(25), 10);
    line(grid, center, body_y(25), center + 2, body_y(23), 10);
    rect(grid, center - 1, body_y(25), 2, 5, 6);
    rect(grid, center - 5, body_y(27), 3, 5, 10);
    rect(grid, center - 4, body_y(28), 1, 3, 15);
    pixel(grid, center, body_y(27), 11);
    pixel(grid, center, body_y(30), 11);
    pixel(grid, center + 3, body_y(28), 10);
    if (elven) {
        pixel(grid, center - 4, body_y(24), 10);
        pixel(grid, center - 3, body_y(25), 10);
        pixel(grid, center - 2, body_y(26), 10);
        pixel(grid, center + 3, body_y(24), 10);
        pixel(grid, center + 2, body_y(25), 10);
        pixel(grid, center + 1, body_y(26), 10);
        pixel(grid, center, body_y(28), 14);
        pixel(grid, center, body_y(29), 10);
    }
    if (race == "gharuk") {
        for (int offset = 0; offset < 3; ++offset) {
            pixel(grid, center - half_body - 1 + offset, body_y(23 + offset), 10);
            pixel(grid, center + half_body + 1 - offset, body_y(23 + offset), 10);
        }
        pixel(grid, center - 3, body_y(26), 3);
        pixel(grid, center + 3, body_y(26), 3);
    }
    rect(grid, center - half_body + 1, body_y(34), half_body * 2 - 2, 2, 1);
    rect(grid, center - half_body + 2, body_y(34), half_body * 2 - 4, 1, 10);
    pixel(grid, center - half_body + 2, body_y(35), 11);
    pixel(grid, center + half_body - 3, body_y(35), 11);

    const int leg_top = body_y(36);
    const int leg_height = 44 - leg_top;
    const int leg_gap = child ? 2 : 3;
    const int leg_width = child ? 3 : 4;
    const int left_leg = center - leg_gap / 2 - leg_width + stride / 2;
    const int right_leg = center + (leg_gap + 1) / 2 + stride / 2;
    rect(grid, left_leg - 1, leg_top, leg_width + 1, leg_height, 1);
    rect(grid, right_leg, leg_top, leg_width + 1, leg_height, 1);
    rect(grid, left_leg, leg_top + 1, leg_width - 1, leg_height - 2, 8);
    rect(grid, right_leg + 1, leg_top + 1, leg_width - 1, leg_height - 2, 8);
    pixel(grid, left_leg + 1, leg_top + 2, 9);
    pixel(grid, right_leg + 1, leg_top + 2, 9);
    if (skirt) {
        rect(grid, torso_left - 1, body_y(34), torso_width + 2, 5, 8);
        rect(grid, torso_left + 1, body_y(35), 2, 3, 9);
        rect(grid, torso_left + torso_width - 3, body_y(35), 2, 3, 9);
        rect(grid, torso_left + 5, body_y(35), 2, 1, 10);
    }

    rect(grid, left_leg - 1, 43, leg_width + 2, 4, 1);
    rect(grid, left_leg, 44, leg_width, 2, 8);
    rect(grid, left_leg, 44, leg_width - 1, 1, 11);
    rect(grid, right_leg, 43, leg_width + 2, 4, 1);
    rect(grid, right_leg + 1, 44, leg_width, 2, 8);
    rect(grid, right_leg + 1, 44, leg_width - 1, 1, 11);
    pixel(grid, left_leg + 1, 45, 15);
    pixel(grid, right_leg + 2, 45, 15);

    if (parts.contains("accessory") && parts["accessory"].is_array()) {
        for (const Json& accessory : parts["accessory"].as_array()) {
            if (!accessory.is_string()) continue;
            if (accessory.as_string() == "circlet" || accessory.as_string() == "headband") {
                rect(grid, head_cx - 5, head_y(9), 10, 1, 10);
                pixel(grid, head_cx, head_y(8), 11);
                pixel(grid, head_cx, head_y(7), 10);
            } else if (accessory.as_string() == "scarf") {
                rect(grid, center + 2, body_y(23), 4, 2, 10);
                pixel(grid, center + 4, body_y(25), 10);
                pixel(grid, center + 5, body_y(26), 10);
                pixel(grid, center + 6, body_y(27), 14);
            }
        }
    }

    return grid;
}

} // namespace

Json compile_character(const Json& spec, const Json&) {
    Json graphic = new_graphic(48, 64, {}, {24, 62});
    graphic["palette"] = build_palette(spec["colors"]);

    JsonObject frames;
    const std::vector<std::string> directions = {
        "down", "down_right", "right", "up_right", "up",
        "up_left", "left", "down_left"
    };
    for (const std::string& direction : directions) {
        std::string facing = direction;
        bool mirrored = false;
        if (direction == "left") { facing = "right"; mirrored = true; }
        else if (direction == "up_left") { facing = "up_right"; mirrored = true; }
        else if (direction == "down_left") { facing = "down_right"; mirrored = true; }
        for (int step = 0; step < 3; ++step) {
            auto grid = character_frame(spec, facing, step);
            if (mirrored) grid = mirror_grid(grid);
            frames[direction + "_" + std::to_string(step)] = rows_json(grid);
        }
    }
    graphic["frames"] = frames;

    JsonObject animations;
    for (const std::string& direction : directions) {
        JsonArray walk_frames;
        walk_frames.emplace_back(direction + "_1");
        walk_frames.emplace_back(direction + "_0");
        walk_frames.emplace_back(direction + "_2");
        walk_frames.emplace_back(direction + "_0");
        animations["walk_" + direction] = Json(JsonObject{{"frames", walk_frames}, {"fps", 8}});
        animations["idle_" + direction] = Json(JsonObject{{"frames", JsonArray{direction + "_0"}}, {"fps", 1}});
    }
    graphic["anims"] = animations;
    Json character_spec = spec;
    if (character_spec.is_object() && character_spec.contains("parts") &&
        character_spec["parts"].is_object()) {
        character_spec["parts"].as_object().erase("weapon");
    }
    graphic["spec"] = std::move(character_spec);
    return graphic;
}

} // namespace aur
