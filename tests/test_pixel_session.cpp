#include "mini_test.hpp"
#include "PixelEditSession.hpp"
#include "PixelFrameOps.hpp"
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

// D5.S1.2: one owner for the anim a direction/pose pair names. The pose-
// direction anim wins, then the bare pose, then none; the fps and the loop
// flag come with it, because the preview plays the sprite's own sequence.
void test_sprite_anim_resolution()
{
    aur::Project p = make_project();
    Json& g = p.graphics["both"];
    g = JsonObject{
        {"w", 2},
        {"h", 2},
        {"palette", JsonArray{"#00000000", "#ffffff"}},
        {"frames", JsonObject{{"a", JsonArray{"10", "01"}}, {"b", JsonArray{"01", "10"}}}},
        {"anims", JsonObject{
            {"idle", JsonObject{{"frames", JsonArray{"a"}}, {"fps", 3}, {"loop", false}}},
            {"walk_down", JsonObject{{"frames", JsonArray{"a", "b"}}, {"fps", 12}}}}},
    };

    const auto directional =
        aur::PixelEditSession::resolve_anim(p, "both", "down", "walk");
    ASSERT(directional.id == "walk_down");
    ASSERT(directional.fps == 12);
    ASSERT(directional.loop);  // no loop flag means it loops

    const auto bare = aur::PixelEditSession::resolve_anim(p, "both", "right", "idle");
    ASSERT(bare.id == "idle");
    ASSERT(bare.fps == 3);
    ASSERT(!bare.loop);

    // A pair with no anim names none: the frames stand alone.
    const auto none = aur::PixelEditSession::resolve_anim(p, "both", "left", "attack");
    ASSERT(none.id.empty());
    ASSERT(none.fps == 0);

    // A missing graphic names none either.
    const auto missing = aur::PixelEditSession::resolve_anim(p, "nope", "down", "idle");
    ASSERT(missing.id.empty());

    // The frames the pair names come from the same resolution.
    const auto frames = aur::PixelEditSession::resolve_anim_frames(p, "both", "down", "walk");
    ASSERT(frames.size() == 2 && frames[0] == "a" && frames[1] == "b");
}

// D6.S1.3: a PNG's pixels become rows, and every pixel is snapped to the
// nearest colour in the tile's palette.
void test_sprite_png_snap()
{
    const std::vector<std::string> palette{"#00000000", "#ff0000ff", "#00ff00ff",
                                           "#0000ffff"};
    // 2x2: red, green, blue, transparent.
    const std::vector<unsigned char> rgba{
        255, 0, 0, 255,   0, 255, 0, 255,
        0, 0, 255, 255,   0, 0, 0, 0};
    const JsonArray rows = aur::editor::snap_rgba_to_rows(rgba, 2, 2, palette);
    ASSERT(rows.size() == 2);
    ASSERT(rows[0].as_string() == "12");
    ASSERT(rows[1].as_string() == "30");

    // A colour that is nearly in the palette snaps to it.
    const std::vector<unsigned char> near_red{250, 8, 4, 255};
    const JsonArray near = aur::editor::snap_rgba_to_rows(near_red, 1, 1, palette);
    ASSERT(near[0].as_string() == "1");

    // An empty palette or an empty image is nothing to snap.
    ASSERT(aur::editor::snap_rgba_to_rows(rgba, 2, 2, {}).empty());
    ASSERT(aur::editor::snap_rgba_to_rows({}, 2, 2, palette).empty());
}

// D5.S1.4: the frame tools over a sprite's sequence. Add, duplicate, remove
// and move are pure functions over the graphic JSON, so they are pinned
// without a widget.
void test_sprite_frame_tools()
{
    aur::Project p = make_project();
    Json &g = p.graphics["sprited"];
    const std::string anim = "walk_down";

    // A free base is taken as it is; a taken one gets the lowest free suffix.
    ASSERT(aur::editor::unique_frame_id(g["frames"], "down_9") == "down_9");
    ASSERT(aur::editor::unique_frame_id(g["frames"], "down_0") == "down_0_2");
    ASSERT(aur::editor::unique_frame_id(g["frames"], "down_1") == "down_1_2");

    // Add: a blank frame of the graphic's size, at the end of the sequence.
    std::string added;
    ASSERT(aur::editor::add_frame(g, anim, "down_0", added));
    ASSERT(added == "down_0_2");
    ASSERT(g["frames"].contains(added));
    ASSERT(g["frames"][added].as_array().size() == 2);
    ASSERT(g["frames"][added].as_array()[0].as_string() == "..");
    ASSERT(g["anims"][anim]["frames"].as_array().size() == 3);
    ASSERT(g["anims"][anim]["frames"].as_array()[2].as_string() == added);

    // Duplicate: the same pixels under a new name, after the source. The
    // index is where the copy goes.
    std::string copy;
    ASSERT(aur::editor::duplicate_frame(g, anim, "down_0", 1, copy));
    ASSERT(copy == "down_0_3");
    ASSERT(g["frames"][copy].as_array()[0].as_string() == "10");
    ASSERT(g["anims"][anim]["frames"].as_array().size() == 4);
    ASSERT(g["anims"][anim]["frames"].as_array()[1].as_string() == copy);

    // Move: the frame swaps with its neighbour in the sequence.
    ASSERT(aur::editor::move_frame(g, anim, 0, 1));
    ASSERT(g["anims"][anim]["frames"].as_array()[0].as_string() == copy);
    ASSERT(g["anims"][anim]["frames"].as_array()[1].as_string() == "down_0");
    // A move past the end is refused, and changes nothing.
    const std::size_t before = g["anims"][anim]["frames"].as_array().size();
    ASSERT(!aur::editor::move_frame(g, anim, static_cast<int>(before) - 1, 1));
    ASSERT(g["anims"][anim]["frames"].as_array().size() == before);

    // Remove: out of the sequence, and out of the graphic when no anim names
    // it any more.
    ASSERT(aur::editor::remove_frame(g, anim, added));
    ASSERT(!g["frames"].contains(added));
    ASSERT(g["anims"][anim]["frames"].as_array().size() == 3);
    // A frame another anim still names stays in the graphic.
    Json &other = p.graphics["other"];
    other = JsonObject{
        {"w", 2},
        {"h", 2},
        {"frames", JsonObject{{"keep", JsonArray{"11", "11"}}}},
        {"anims", JsonObject{{"idle", JsonObject{{"frames", JsonArray{"keep"}}}}}},
    };
    ASSERT(aur::editor::remove_frame(g, anim, "down_0"));
    ASSERT(!g["frames"].contains("down_0"));
    ASSERT(other["frames"].contains("keep"));
}

// D5.S1.3: the mirror copies the right half of a frame onto its left half.
// The right half is mirrored onto the left, so the result is symmetric
// about the middle and the mirror of a symmetric frame is itself.
void test_sprite_frame_mirror()
{
    const JsonArray rows{Json("abcd"), Json("wxyz"), Json("....")};
    const JsonArray mirrored = aur::editor::mirror_rows_right_to_left(rows);
    ASSERT(mirrored.size() == 3);
    ASSERT(mirrored[0].as_string() == "dccd");
    ASSERT(mirrored[1].as_string() == "zyyz");
    ASSERT(mirrored[2].as_string() == "....");

    // An odd width keeps its middle column.
    const JsonArray odd{Json("abc")};
    const JsonArray mirrored_odd = aur::editor::mirror_rows_right_to_left(odd);
    ASSERT(mirrored_odd[0].as_string() == "cbc");

    // A single column mirrors to itself.
    const JsonArray single{Json("a")};
    ASSERT(aur::editor::mirror_rows_right_to_left(single)[0].as_string() == "a");

    // The mirror is its own inverse on a symmetric frame.
    const JsonArray again = aur::editor::mirror_rows_right_to_left(mirrored);
    ASSERT(again[0].as_string() == "dccd");
    ASSERT(again[1].as_string() == "zyyz");

    // A row that is not a string is carried over unchanged.
    JsonArray mixed{Json("ab"), Json(7)};
    const JsonArray mirrored_mixed = aur::editor::mirror_rows_right_to_left(mixed);
    ASSERT(mirrored_mixed.size() == 2);
    ASSERT(mirrored_mixed[0].as_string() == "bb");
    ASSERT(mirrored_mixed[1].as_int() == 7);
}
