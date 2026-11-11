#pragma once

// PixelFrameOps.hpp - the sprite editor's structural edits on a graphic's
// frames, as pure functions over the project JSON.
//
// These are the edits that change the shape of a sprite rather than its
// pixels: mirroring a frame, adding one, removing one, moving one in the
// sequence. They are functions and not methods so that they can be tested
// without a widget, and so that the canvas stays the only owner of the
// document.

#include "aur/json.hpp"

namespace aur::editor
{

    // Mirrors the right half of a frame onto its left half: the left column
    // takes the right column's mirror image, and the middle column of an odd
    // width keeps its own pixel. A row that is not a string is carried over
    // unchanged.
    JsonArray mirror_rows_right_to_left(const JsonArray &rows);

    // A frame id that is not taken: the base when it is free, otherwise the
    // base with the lowest free "_2", "_3", ... suffix.
    std::string unique_frame_id(const Json &frames, const std::string &base);

    // Adds a blank frame (the graphic's size, empty) and names it. The id
    // goes into new_id. When the graphic has an anim, the frame joins the
    // end of that anim's sequence.
    bool add_frame(Json &graphic, const std::string &anim_id,
                   const std::string &base_id, std::string &new_id);

    // Copies a frame's pixels into a new frame and puts it after the source
    // in the anim's sequence.
    bool duplicate_frame(Json &graphic, const std::string &anim_id,
                         const std::string &source_id, int source_index,
                         std::string &new_id);

    // Takes a frame out of the anim's sequence, and out of the graphic when
    // no anim names it any more.
    bool remove_frame(Json &graphic, const std::string &anim_id,
                      const std::string &frame_id);

    // Moves a frame one place along the anim's sequence.
    bool move_frame(Json &graphic, const std::string &anim_id, int index,
                    int delta);

} // namespace aur::editor
