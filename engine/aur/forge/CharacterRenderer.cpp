#include "aur/forge.hpp"
#include "aur/project.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace aur {
namespace {

constexpr int P_OUTLINE = 1;
constexpr int P_SKIN = 2;
constexpr int P_SKIN_SHADE = 3;
constexpr int P_HAIR = 4;
constexpr int P_HAIR_SHADE = 5;
constexpr int P_TOP = 6;
constexpr int P_TOP_SHADE = 7;
constexpr int P_BOT = 8;
constexpr int P_BOT_SHADE = 9;
constexpr int P_TRIM = 10;
constexpr int P_METAL = 11;
constexpr int P_EYE = 12;
constexpr int P_HAIR_HI = 13;
constexpr int P_ACCENT = 14;
constexpr int P_SKIN_HI = 15;

// Native 48x64 canvas: every coordinate below is a real pixel, never scaled.
struct Canvas {
    int w = 48;
    int h = 64;
    std::vector<std::vector<int>> px;

    Canvas(int width, int height)
        : w(width), h(height),
          px(static_cast<size_t>(height), std::vector<int>(static_cast<size_t>(width), 0)) {}

    void set(int x, int y, int c) {
        if (x >= 0 && x < w && y >= 0 && y < h) {
            px[static_cast<size_t>(y)][static_cast<size_t>(x)] = c;
        }
    }

    void rect(int x, int y, int rw, int rh, int c) {
        for (int py = std::max(0, y); py < std::min(h, y + rh); ++py) {
            for (int pxi = std::max(0, x); pxi < std::min(w, x + rw); ++pxi) {
                px[static_cast<size_t>(py)][static_cast<size_t>(pxi)] = c;
            }
        }
    }

    void outlined_rect(int x, int y, int rw, int rh, int fill, int outline = P_OUTLINE) {
        rect(x, y, rw, rh, outline);
        if (rw > 2 && rh > 2) rect(x + 1, y + 1, rw - 2, rh - 2, fill);
    }

    void ellipse(int cx, int cy, int rx, int ry, int c) {
        if (rx <= 0 || ry <= 0) return;
        for (int y = -ry; y <= ry; ++y) {
            const double row = 1.0 - static_cast<double>(y * y) / (ry * ry);
            const int half_width = static_cast<int>(std::floor(rx * std::sqrt(std::max(0.0, row))));
            for (int x = -half_width; x <= half_width; ++x) set(cx + x, cy + y, c);
        }
    }

    void line(int x0, int y0, int x1, int y1, int c) {
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        while (true) {
            set(x0, y0, c);
            if (x0 == x1 && y0 == y1) break;
            const int doubled = 2 * error;
            if (doubled >= dy) { error += dy; x0 += sx; }
            if (doubled <= dx) { error += dx; y0 += sy; }
        }
    }

    void mirror_x() {
        for (auto& row : px) std::reverse(row.begin(), row.end());
    }

    Json to_rows_json() const {
        JsonArray rows;
        constexpr char digits[] = "0123456789ABCDEF";
        for (const auto& row : px) {
            std::string encoded;
            encoded.reserve(row.size());
            for (int pixel : row) encoded.push_back(digits[std::clamp(pixel, 0, 15)]);
            rows.emplace_back(std::move(encoded));
        }
        return Json(rows);
    }
};

std::string part_string(const Json& parts, const std::string& key, const std::string& fallback) {
    return parts.is_object() && parts.contains(key) && parts[key].is_string()
        ? parts[key].as_string() : fallback;
}

struct Rig {
    std::string race;
    std::string variant;
    std::string age;
    std::string presentation;
    std::string hair_style;
    std::string bottom;
    std::string role;
    std::string affinity;

    bool elven = false;
    bool beast = false;
    bool avian = false;
    bool gharuk = false;
    bool dragon = false;
    bool undead = false;
    bool spirit = false;
    bool dryad = false;
    bool siren = false;
    bool vampire = false;

    bool side = false;
    bool three_q = false;
    bool back = false;
    int step = 0;

    int cx = 24;
    int head_shift = 0;
    int bob = 0;
    int head_cy = 16;
    int head_rx = 9;
    int head_ry = 9;
    int torso_top = 26;
    int torso_h = 17;
    int torso_w = 18;
    int leg_top = 43;
    int foot_y = 61;
};

Rig build_rig(const Json& spec, const std::string& facing, int step) {
    Rig r;
    const Json& parts = spec.is_object() && spec.contains("parts") ? spec["parts"] : Json{};
    r.race = part_string(parts, "race", "kaeli");
    r.variant = part_string(parts, "variant", "standard");
    r.age = part_string(parts, "age_group", "adult");
    r.presentation = part_string(parts, "presentation", "androgynous");
    r.hair_style = part_string(parts, "hair_style", "short");
    r.bottom = part_string(parts, "bottom", "pants");
    r.role = part_string(parts, "role", "villager");
    r.affinity = part_string(parts, "affinity", "none");

    r.elven = r.race == "sylvani" || r.race == "solani" ||
              part_string(parts, "ears", "") == "elf" ||
              (r.race == "vampires" && r.variant == "sylvani_derived");
    r.beast = r.race == "beastkin" || r.race == "were_creatures";
    r.avian = r.race == "featherfolk";
    r.gharuk = r.race == "gharuk";
    r.dragon = r.race == "veylkai";
    r.undead = r.race == "undead";
    r.dryad = r.race == "treesprites_dryads";
    r.siren = r.race == "sirens_mermaids";
    r.vampire = r.race == "vampires";
    r.spirit = r.race == "spirits" || r.race == "elementals" || r.race == "fairies_nymphs";

    r.side = facing == "right";
    r.three_q = facing == "down_right" || facing == "up_right";
    r.back = facing == "up" || facing == "up_left" || facing == "up_right";
    r.step = step;
    r.bob = (step == 1 || step == 2) ? -1 : 0;

    if (r.age == "child") {
        r.head_shift = 7;
        r.torso_w = r.side ? 11 : 14;
        r.torso_h = 13;
    } else if (r.age == "teen") {
        r.head_shift = 3;
        r.torso_w = r.side ? 12 : 16;
        r.torso_h = 15;
    } else {
        r.torso_w = r.side ? 13 : (r.presentation == "feminine" ? 16 : 19);
        r.torso_h = 17;
    }

    if (r.gharuk || r.beast) r.torso_w += r.side ? 2 : 3;
    if (r.race == "sylvani" && !r.side) r.torso_w -= 1;
    if (r.avian && !r.side) r.torso_w += 2;

    r.head_cy = 16 + r.head_shift + r.bob;
    r.torso_top = r.head_cy + r.head_ry + 1;
    r.leg_top = r.torso_top + r.torso_h;
    return r;
}

void draw_ground_shadow(Canvas& c, const Rig& r) {
    c.rect(r.cx - 8, r.foot_y - 1, 16, 1, P_OUTLINE);
    c.rect(r.cx - 5, r.foot_y, 10, 1, P_OUTLINE);
}

void draw_back_appendages(Canvas& c, const Rig& r) {
    const int ty = r.torso_top + 4;
    if (r.avian) {
        const int dirs[2] = {-1, 1};
        if (r.side || r.three_q) {
            c.ellipse(r.cx - 8, ty + 6, 4, 9, P_OUTLINE);
            c.ellipse(r.cx - 8, ty + 6, 3, 8, P_HAIR);
            c.line(r.cx - 9, ty + 1, r.cx - 10, ty + 12, P_HAIR_HI);
            c.line(r.cx - 7, ty + 3, r.cx - 8, ty + 13, P_HAIR_SHADE);
        } else {
            for (int dir : dirs) {
                const int wx = r.cx + dir * (r.torso_w / 2 + 4);
                c.ellipse(wx, ty + 6, 4, 9, P_OUTLINE);
                c.ellipse(wx, ty + 6, 3, 8, P_HAIR);
                c.line(wx + dir, ty + 1, wx + dir * 2, ty + 11, P_HAIR_HI);
                c.line(wx - dir, ty + 4, wx, ty + 13, P_HAIR_SHADE);
            }
        }
    }
    if (r.spirit) {
        for (int dir : {-1, 1}) {
            const int wx = r.cx + dir * (r.torso_w / 2 + 4);
            c.ellipse(wx, ty + 2, 4, 6, P_OUTLINE);
            c.ellipse(wx, ty + 2, 3, 5, P_ACCENT);
            c.set(wx + dir, ty, P_SKIN_HI);
        }
    }
    if (r.beast || (r.dragon && r.variant == "dragon_form")) {
        const int tail_x = r.side ? r.cx - 8 : r.cx + r.torso_w / 2 + 2;
        const int tail_y = r.leg_top - 2;
        const int swing = r.side ? -5 : 5;
        c.line(tail_x, tail_y, tail_x + swing, tail_y - 4, P_OUTLINE);
        c.line(tail_x + swing, tail_y - 4, tail_x + swing + (r.side ? -1 : 1), tail_y - 10, P_OUTLINE);
        c.line(tail_x, tail_y - 1, tail_x + swing * 4 / 5, tail_y - 4, r.dragon ? P_ACCENT : P_HAIR);
        c.set(tail_x + swing, tail_y - 9, P_HAIR_HI);
    }
}

void draw_legs_and_boots(Canvas& c, const Rig& r) {
    const int leg_w = (r.age == "child") ? 4 : 5;
    const int gap = r.gharuk ? 4 : (r.age == "child" ? 2 : 3);
    const int leg_bottom = r.foot_y - 1;
    const int stride = (r.step == 1) ? -2 : (r.step == 2 ? 2 : 0);

    int left_x = r.cx - gap / 2 - leg_w;
    int right_x = r.cx + (gap + 1) / 2;
    int left_drop = 0;
    int right_drop = 0;
    if (r.side) {
        left_x = r.cx - leg_w / 2 + stride;
        right_x = r.cx - leg_w / 2 - stride;
    } else {
        if (r.step == 1) left_drop = -2;
        if (r.step == 2) right_drop = -2;
    }

    const auto draw_leg = [&](int x, int drop, bool shaded) {
        const int top = r.leg_top + (drop < 0 ? -1 : 0);
        const int bottom = leg_bottom + drop;
        const int height = std::max(4, bottom - top + 1);
        c.outlined_rect(x - 1, top, leg_w + 2, height, P_BOT);
        c.rect(x + (shaded ? leg_w - 1 : 0), top + 1, 1, std::max(1, height - 2), P_BOT_SHADE);
        const int boot_top = std::max(top + 2, bottom - 3);
        c.outlined_rect(x - 1, boot_top, leg_w + 2, bottom - boot_top + 1, P_BOT_SHADE);
        c.rect(x, boot_top + 1, leg_w, 1, P_TRIM);
        c.set(x + 1, boot_top + 2, P_METAL);
    };

    draw_leg(left_x, left_drop, false);
    draw_leg(right_x, right_drop, true);

    const bool skirt = r.bottom == "skirt" ||
                       (r.presentation == "feminine" && r.bottom != "pants" && r.bottom != "shorts");
    if (skirt) {
        const int sw = r.torso_w + 2;
        const int sx = r.cx - sw / 2;
        c.outlined_rect(sx, r.leg_top - 2, sw, 8, P_BOT);
        c.rect(sx + 2, r.leg_top, 2, 4, P_BOT_SHADE);
        c.rect(sx + sw - 4, r.leg_top, 2, 4, P_BOT_SHADE);
        c.rect(sx + 1, r.leg_top + 4, sw - 2, 1, P_TRIM);
    }
}

void draw_torso_and_arms(Canvas& c, const Rig& r) {
    const int hw = r.torso_w / 2;
    const int arm_swing = (r.step == 1) ? 2 : (r.step == 2 ? -2 : 0);

    if (r.side) {
        const int ax = r.cx - 1 + arm_swing;
        c.outlined_rect(ax - 2, r.torso_top + 2, 5, r.torso_h - 1, P_TOP);
        c.outlined_rect(ax - 2, r.torso_top + r.torso_h - 3, 5, 4, P_SKIN);
        c.set(ax, r.torso_top + r.torso_h - 2, P_SKIN_SHADE);
    } else {
        c.outlined_rect(r.cx - hw - 3, r.torso_top + 2 + arm_swing, 5, r.torso_h - 3, P_TOP);
        c.outlined_rect(r.cx + hw - 1, r.torso_top + 2 - arm_swing, 5, r.torso_h - 3, P_TOP_SHADE);
        c.outlined_rect(r.cx - hw - 2, r.torso_top + r.torso_h - 3 + arm_swing, 4, 4, P_SKIN);
        c.outlined_rect(r.cx + hw - 1, r.torso_top + r.torso_h - 3 - arm_swing, 4, 4, P_SKIN_SHADE);
    }

    c.rect(r.cx - 3, r.torso_top - 2, 6, 3, P_OUTLINE);
    c.rect(r.cx - 2, r.torso_top - 2, 4, 3, P_SKIN_SHADE);
    c.rect(r.cx - 1, r.torso_top - 2, 2, 2, P_SKIN);

    for (int row = 0; row < r.torso_h; ++row) {
        int row_hw = hw;
        if (row < 2) row_hw = std::max(4, hw - (2 - row));
        else if (r.presentation == "feminine" && row >= 6 && row <= 10) row_hw = std::max(4, hw - 1);
        const int y = r.torso_top + row;
        c.rect(r.cx - row_hw - 1, y, row_hw * 2 + 2, 1, P_OUTLINE);
        c.rect(r.cx - row_hw, y, row_hw * 2, 1, P_TOP);
        c.rect(r.cx + row_hw - 2, y, 2, 1, P_TOP_SHADE);
        if (row >= r.torso_h - 2) c.rect(r.cx - row_hw, y, row_hw * 2, 1, P_TOP_SHADE);
    }

    if (!r.back) {
        c.line(r.cx - 3, r.torso_top + 1, r.cx, r.torso_top + 4, P_TRIM);
        c.line(r.cx, r.torso_top + 4, r.cx + 3, r.torso_top + 1, P_TRIM);
        if (r.race == "sylvani") {
            c.set(r.cx, r.torso_top + 5, P_ACCENT);
            c.rect(r.cx - 1, r.torso_top + 7, 2, 4, P_TRIM);
        } else if (r.race == "solani") {
            c.line(r.cx - hw + 2, r.torso_top + 2, r.cx + hw - 3, r.torso_top + r.torso_h - 4, P_TRIM);
        } else if (r.gharuk) {
            c.rect(r.cx - hw, r.torso_top + 1, 3, 3, P_METAL);
            c.rect(r.cx + hw - 3, r.torso_top + 1, 3, 3, P_METAL);
        } else if (r.vampire) {
            c.rect(r.cx - 5, r.torso_top - 1, 2, 3, P_ACCENT);
            c.rect(r.cx + 3, r.torso_top - 1, 2, 3, P_ACCENT);
        } else if (r.role == "mage") {
            c.rect(r.cx - 1, r.torso_top + 6, 2, r.torso_h - 9, P_ACCENT);
        } else if (r.role == "guard") {
            c.rect(r.cx - hw + 2, r.torso_top + 3, (hw - 2) * 2, 2, P_METAL);
        } else if (r.role == "adventurer") {
            c.line(r.cx - hw + 3, r.torso_top + 3, r.cx + hw - 4, r.torso_top + r.torso_h - 5, P_BOT_SHADE);
        }
    }

    const int belt_y = r.torso_top + r.torso_h - 3;
    c.rect(r.cx - hw, belt_y, hw * 2, 2, P_OUTLINE);
    c.rect(r.cx - hw + 1, belt_y, hw * 2 - 2, 1, P_TRIM);
    if (!r.back) c.rect(r.cx - 2, belt_y, 4, 2, P_METAL);
}

void draw_ears(Canvas& c, const Rig& r) {
    if (r.elven) {
        const int tip_y = 13 + r.head_shift + r.bob;
        for (int dir : {-1, 1}) {
            if (r.side && dir < 0) continue;
            const int tip_x = (dir < 0) ? 7 : 40;
            const int base_x = r.cx + dir * (r.head_rx - 1);
            c.line(base_x, tip_y + 2, tip_x, tip_y, P_OUTLINE);
            c.line(base_x, tip_y + 5, tip_x + (dir < 0 ? 1 : -1), tip_y + 1, P_OUTLINE);
            c.line(base_x, tip_y + 3, tip_x + (dir < 0 ? 2 : -2), tip_y + 1, P_SKIN);
            c.line(base_x, tip_y + 4, tip_x + (dir < 0 ? 3 : -3), tip_y + 2, P_SKIN_SHADE);
            c.set(tip_x, tip_y, P_OUTLINE);
        }
    } else if (r.siren) {
        for (int dir : {-1, 1}) {
            const int ex = r.cx + dir * (r.head_rx + 1);
            c.rect(ex - 1, r.head_cy - 1, 3, 5, P_OUTLINE);
            c.rect(ex, r.head_cy, 1, 3, P_ACCENT);
        }
    } else if (!r.beast) {
        for (int dir : {-1, 1}) {
            if (r.side && dir < 0) continue;
            const int ex = r.cx + dir * r.head_rx;
            c.rect(ex - 1, r.head_cy - 1, 3, 4, P_OUTLINE);
            c.rect(ex, r.head_cy, 1, 2, dir < 0 ? P_SKIN : P_SKIN_SHADE);
        }
    }
}

void draw_head_and_face(Canvas& c, const Rig& r) {
    draw_ears(c, r);

    c.ellipse(r.cx, r.head_cy, r.head_rx + 1, r.head_ry + 1, P_OUTLINE);
    if (r.back) {
        c.ellipse(r.cx, r.head_cy, r.head_rx, r.head_ry, P_HAIR);
        c.ellipse(r.cx, r.head_cy - 3, r.head_rx - 2, 3, P_HAIR_HI);
        c.rect(r.cx - r.head_rx + 2, r.head_cy + 3, (r.head_rx - 2) * 2, 5, P_HAIR_SHADE);
        return;
    }

    c.ellipse(r.cx, r.head_cy, r.head_rx, r.head_ry, P_SKIN);
    c.rect(r.cx - 4, r.head_cy + r.head_ry - 2, 8, 1, P_SKIN_SHADE);
    c.rect(r.cx + r.head_rx - 2, r.head_cy, 2, 4, P_SKIN_SHADE);

    const int eye_y = r.head_cy;
    const auto draw_eye = [&](int ex) {
        c.rect(ex, eye_y - 1, 3, 1, P_OUTLINE);
        c.rect(ex, eye_y, 3, 3, P_OUTLINE);
        c.rect(ex + 1, eye_y, 2, 2, P_EYE);
        c.set(ex + 1, eye_y, P_SKIN_HI);
    };

    if (r.side) {
        draw_eye(r.cx + 4);
        c.set(r.cx + 7, eye_y + 2, P_SKIN_SHADE);
        c.rect(r.cx + 4, eye_y + 4, 2, 1, P_SKIN_SHADE);
    } else if (r.three_q) {
        draw_eye(r.cx - 2);
        draw_eye(r.cx + 4);
        c.set(r.cx + 3, eye_y + 3, P_SKIN_SHADE);
        c.rect(r.cx + 1, eye_y + 5, 3, 1, P_SKIN_SHADE);
    } else {
        draw_eye(r.cx - 6);
        draw_eye(r.cx + 3);
        c.set(r.cx, eye_y + 3, P_SKIN_SHADE);
        c.rect(r.cx - 1, eye_y + 5, 3, 1, P_SKIN_SHADE);
    }

    if (r.avian) {
        const int bx = r.side ? r.cx + 7 : (r.three_q ? r.cx + 2 : r.cx - 1);
        c.outlined_rect(bx, eye_y + 2, 4, 3, P_TRIM);
    } else if (r.beast) {
        const int nx = r.side ? r.cx + 6 : (r.three_q ? r.cx + 2 : r.cx - 1);
        c.rect(nx, eye_y + 3, 2, 2, P_OUTLINE);
    } else if (r.gharuk) {
        if (!r.side) {
            c.set(r.cx - 2, eye_y + 5, P_SKIN_HI);
            c.set(r.cx + 2, eye_y + 5, P_SKIN_HI);
        }
    }
}

struct BeastEarStyle {
    const char* variant;
    int rx;
    int ry;
    bool round;
    int tip;
};

constexpr BeastEarStyle kBeastEars[] = {
    {"wolf",  3, 5, false, P_HAIR_HI},
    {"cat",   3, 4, false, P_HAIR_SHADE},
    {"dog",   4, 3, true,  P_HAIR_SHADE},
    {"tiger", 4, 5, false, P_TRIM},
    {"lion",  4, 6, true,  P_HAIR_HI},
    {"bear",  4, 4, true,  P_HAIR}
};

const BeastEarStyle& beast_ear_style(const std::string& variant) {
    for (const BeastEarStyle& style : kBeastEars) {
        if (variant == style.variant) return style;
    }
    return kBeastEars[5];
}

void draw_hair_and_crown(Canvas& c, const Rig& r, const Json& parts) {
    const int crown_y = r.head_cy - r.head_ry;

    c.ellipse(r.cx, r.head_cy - r.head_ry + 3, r.head_rx - 1, 4, P_HAIR);
    c.rect(r.cx - 5, crown_y + 2, 10, 1, P_HAIR_HI);
    c.rect(r.cx - 6, crown_y + 5, 12, 1, P_HAIR_SHADE);
    if (!r.back) {
        c.rect(r.cx - 6, crown_y + 5, 3, 2, P_HAIR);
        c.rect(r.cx + 4, crown_y + 5, 3, 2, P_HAIR);
    }

    const int lock_h = (r.hair_style == "long") ? 17 : (r.hair_style == "curly" ? 11 : 6);
    if (!r.back) {
        c.rect(r.cx - r.head_rx - 1, crown_y + 3, 3, lock_h, P_HAIR);
        c.rect(r.cx + r.head_rx - 1, crown_y + 3, 3, lock_h, P_HAIR_SHADE);
        if (r.hair_style == "curly") {
            c.ellipse(r.cx - r.head_rx + 1, crown_y + 4, 2, 3, P_HAIR);
            c.ellipse(r.cx + r.head_rx - 1, crown_y + 4, 2, 3, P_HAIR_SHADE);
        }
    } else {
        c.ellipse(r.cx, r.head_cy - 1, r.head_rx, r.head_ry - 1, P_HAIR);
        const int back_h = (r.hair_style == "long") ? 20 : 10;
        c.rect(r.cx - r.head_rx + 2, r.head_cy + 2, (r.head_rx - 2) * 2, back_h, P_HAIR);
        c.rect(r.cx - 4, r.head_cy + 6, 8, std::max(2, back_h - 6), P_HAIR_SHADE);
        c.ellipse(r.cx, r.head_cy - 4, r.head_rx - 3, 2, P_HAIR_HI);
    }
    if (r.hair_style == "long" && !r.back) {
        c.outlined_rect(r.cx - r.head_rx - 1, r.head_cy + 2, 3, 12, P_HAIR);
        c.outlined_rect(r.cx + r.head_rx - 1, r.head_cy + 2, 3, 12, P_HAIR_SHADE);
    }

    if (r.beast) {
        const BeastEarStyle& style = beast_ear_style(r.variant);
        for (int dir : {-1, 1}) {
            const int ex = r.cx + dir * 6;
            if (style.round) {
                c.ellipse(ex, crown_y, style.rx, style.ry, P_OUTLINE);
                c.ellipse(ex, crown_y, style.rx - 1, style.ry - 1, P_HAIR);
                c.set(ex, crown_y - 1, style.tip);
            } else {
                c.line(ex - dir * 2, crown_y + 1, ex + dir * 2, crown_y - style.ry, P_OUTLINE);
                c.line(ex + dir * 2, crown_y - style.ry, ex + dir * 3, crown_y + 2, P_OUTLINE);
                c.rect(ex, crown_y - 1, 2, 3, P_HAIR);
                c.set(ex, crown_y - style.ry + 1, style.tip);
            }
        }
    }
    if (r.dragon) {
        for (int dir : {-1, 1}) {
            const int hx = r.cx + dir * 6;
            c.line(hx, crown_y + 1, hx + dir * 4, crown_y - 4, P_OUTLINE);
            c.line(hx, crown_y, hx + dir * 3, crown_y - 3, P_TRIM);
        }
    }
    if (r.dryad) {
        c.rect(r.cx - 6, crown_y, 3, 2, P_ACCENT);
        c.rect(r.cx + 3, crown_y, 3, 2, P_ACCENT);
    }
    if (r.undead && r.variant == "lich") {
        c.rect(r.cx - 5, crown_y - 1, 10, 2, P_TRIM);
        c.set(r.cx, crown_y - 2, P_ACCENT);
    }

    if (parts.is_object() && parts.contains("accessory") && parts["accessory"].is_array()) {
        for (const Json& accessory : parts["accessory"].as_array()) {
            if (!accessory.is_string()) continue;
            const std::string& kind = accessory.as_string();
            if (kind == "circlet" || kind == "headband") {
                c.rect(r.cx - 7, crown_y + 4, 14, 2, P_TRIM);
                c.set(r.cx, crown_y + 4, P_ACCENT);
            } else if (kind == "scarf") {
                c.outlined_rect(r.cx - 6, r.torso_top - 1, 12, 4, P_ACCENT);
                c.outlined_rect(r.cx + 2, r.torso_top + 2, 4, 6, P_ACCENT);
            }
        }
    }
}

Canvas render_character_frame(const Json& spec, const std::string& facing, int step) {
    Canvas c(48, 64);
    const Rig r = build_rig(spec, facing, step);
    draw_ground_shadow(c, r);
    draw_back_appendages(c, r);
    draw_legs_and_boots(c, r);
    draw_torso_and_arms(c, r);
    draw_head_and_face(c, r);
    draw_hair_and_crown(c, r, spec.is_object() && spec.contains("parts") ? spec["parts"] : Json{});
    return c;
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
            Canvas canvas = render_character_frame(spec, facing, step);
            if (mirrored) canvas.mirror_x();
            frames[direction + "_" + std::to_string(step)] = canvas.to_rows_json();
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
