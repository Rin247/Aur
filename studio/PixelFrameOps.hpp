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

} // namespace aur::editor
