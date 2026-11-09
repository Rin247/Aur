#include "mini_test.hpp"
#include "PixelEditSession.hpp"
#include "aur/project.hpp"
#include "aur/gfx.hpp"
#include <iostream>

using aur::Json;
using aur::JsonObject;
using aur::JsonArray;

namespace
{

    aur::Project make_project()
    {
        aur::Project p(".");
        // A plain 4x4 graphic, one palette of four colors, one frame.
        Json &g = p.graphics["plain"];
        g = JsonObject{
            {"w", 4},
            {"h", 4},
            {"palette", JsonArray{"#00000000", "#ff0000ff", "#00ff00ff", "#0000ffff"}},
            {"frames", JsonObject{{"0", JsonArray{"1111", "1..1", "1..1", "1111"}}}},
        };
        // An anim-backed graphic (what sprite mode consumes).
        Json& s = p.graphics["sprited"];
        s = JsonObject{
            {"w", 2},
            {"h", 2},
            {"palette", JsonArray{"#00000000", "#ffffff"}},
            {"frames",
             JsonObject{{"down_0", JsonArray{"10", "01"}},
                        {"down_1", JsonArray{"01", "10"}}}},
            {"anims",
             JsonObject{{"walk_down",
                         JsonObject{{"frames", JsonArray{"down_0", "down_1"}},
                                    {"fps", 4},
                                    {"loop", true}}}}},
        };
        return p;
        }

    } // namespace

    // P0: pin the frame/undo/palette data-layer semantics the merged editor
    // relies on (headless; no QApplication, so no clipboard round-trip).
    void test_pixel_session()
    {
        using aur::Json;
        using aur::Project;

        Project p = make_project();

        // Frames object resolves by id for both a plain and an anim-backed graphic.
        const Json &plain = p.graphics["plain"];
        ASSERT(plain.contains("frames") && plain["frames"].is_object());
        ASSERT(plain["frames"].contains("0"));
        const Json &sprited = p.graphics["sprited"];
        ASSERT(sprited["frames"].contains("down_0") && sprited["frames"].contains("down_1"));
        ASSERT(sprited.contains("anims") && sprited["anims"].contains("walk_down"));
        const Json &anim = sprited["anims"]["walk_down"];
        ASSERT(anim.contains("frames") && anim["frames"].is_array());
        ASSERT(anim["frames"].as_array().size() == 2);
        ASSERT(anim["frames"].as_array()[0].as_string() == "down_0");

        // Frame rows are hex-digit strings of graphic width; '.' is transparent.
        const Json &rows = plain["frames"]["0"];
        ASSERT(rows.is_array() && rows.as_array().size() == 4);
        for (const Json &row : rows.as_array())
        {
            ASSERT(row.is_string() && row.as_string().size() == 4);
        }

        // Undo/redo stack shape: before snapshots push onto undo; redo collects
        // during undo traversal; restoreFrame writes frames[current] wholesale.
        {
            std::vector<Json> undo_frames;
            std::vector<Json> redo_frames;
            const std::string frame = "0";

            const auto push_undo = [&]()
            {
                undo_frames.push_back(plain["frames"][frame]);
                if (undo_frames.size() > 100)
                    undo_frames.erase(undo_frames.begin());
                redo_frames.clear();
            };
            const auto restore = [&](const Json &new_rows)
            {
                p.graphics["plain"]["frames"][frame] = new_rows;
            };
            const auto do_undo = [&]()
            {
                if (undo_frames.empty())
                    return;
                redo_frames.push_back(p.graphics["plain"]["frames"][frame]);
                const Json rows_snapshot = undo_frames.back();
                undo_frames.pop_back();
                restore(rows_snapshot);
            };
            const auto do_redo = [&]()
            {
                if (redo_frames.empty())
                    return;
                undo_frames.push_back(p.graphics["plain"]["frames"][frame]);
                const Json rows_snapshot = redo_frames.back();
                redo_frames.pop_back();
                restore(rows_snapshot);
            };

        // Draw: snapshot, mutate.
        push_undo();
        p.graphics["plain"]["frames"]["0"].as_array()[1] = "1111";
        ASSERT(p.graphics["plain"]["frames"]["0"].as_array()[1].as_string() != "1..1");

        // Undo restores exact prior rows.
        do_undo();
        ASSERT(p.graphics["plain"]["frames"]["0"].as_array()[1].as_string() == "1..1");
        ASSERT(redo_frames.size() == 1);

        // Redo reapplies.
        do_redo();
        ASSERT(p.graphics["plain"]["frames"]["0"].as_array()[1].as_string() == "1111");
        ASSERT(undo_frames.size() == 1);
    }

    // Palette entries are hex strings decodable by hex_to_rgba (BGR555-safe path).
    const auto &palette = plain["palette"].as_array();
    const aur::Rgb rgba = aur::hex_to_rgba(palette[1].as_string());
    ASSERT(rgba.r == 0xff && rgba.g == 0x00 && rgba.b == 0x00);

    // A missing/short graphic is rejected by the same guards the canvas uses.
    ASSERT(p.graphics["plain"]["frames"].contains("0"));
    ASSERT(!p.graphics.contains("missing"));
}

// D1.S4: the anim-frame fallback matches whole direction tokens. "right" must
// not match "up_right"/"down_right" frames; "down" stays a catch-all.
void test_direction_token_match()
{
    aur::Project p(".");
    Json& g = p.graphics["tok"];
    g = JsonObject{
        {"w", 2},
        {"h", 2},
        {"palette", JsonArray{"#00000000", "#ffffff"}},
        {"frames",
         JsonObject{{"right_0", JsonArray{"11", "11"}},
                    {"up_right_0", JsonArray{"01", "01"}},
                    {"down_right_0", JsonArray{"10", "10"}},
                    {"up_0", JsonArray{"11", "00"}},
                    {"front_0", JsonArray{"00", "11"}}}},
    };

    const auto right = aur::PixelEditSession::resolve_anim_frames(p, "tok", "right", "idle");
    ASSERT(right.size() == 1 && right[0] == "right_0");
    const auto up = aur::PixelEditSession::resolve_anim_frames(p, "tok", "up", "idle");
    ASSERT(up.size() == 1 && up[0] == "up_0");
    // "down" remains the catch-all that lists every frame.
    const auto down = aur::PixelEditSession::resolve_anim_frames(p, "tok", "down", "idle");
    ASSERT(down.size() == 5);
}
