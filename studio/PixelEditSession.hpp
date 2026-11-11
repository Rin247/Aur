#pragma once

#include <QString>
#include <string>
#include <vector>
#include "aur/project.hpp"

class QListWidget;

namespace aur
{

    // The shared selection/palette/frame data for the pixel and sprite editors
    // (pixmerge P1): one definition of palette listing and anim frame-id
    // resolution instead of the previous per-panel copies. Pure functions
    // over the project JSON — no Qt state of its own beyond the widget fill
    // helpers. The canvas (PixelCanvasWidget) stays the sole edit surface.
    class PixelEditSession
    {
    public:
        // What a direction/pose pair names in a graphic's anims: the anim
        // id, its fps and whether it loops. One owner for the resolution, so
        // the frame list, the preview and the frame tools cannot disagree.
        struct Anim
        {
            std::string id;
            int fps = 0;
            bool loop = true;
        };

        // Fills a palette list widget from graphics[id]["palette"]; returns
        // the number of entries written. Selects row 1 (index 1 = first
        // opaque color) when anything was written.
        static int fill_palette(QListWidget *widget, const Project &project,
                                const std::string &graphic_id);

        // The anim a direction/pose pair names: pose_direction first, then
        // the bare pose, then none. An empty id means the graphic has no
        // anim for the pair and its frames stand alone.
        static Anim resolve_anim(
            const Project &project, const std::string &graphic_id,
            const std::string &direction, const std::string &pose);

        // Resolves the frame ids an anim-backed graphic exposes for a
        // direction/pose pair: pose_direction anim first, then bare pose,
        // then frame ids containing the direction (all frames when the
        // direction is "down"). Returns the deduplicated id list.
        static std::vector<std::string> resolve_anim_frames(
            const Project &project, const std::string &graphic_id,
            const std::string &direction, const std::string &pose);

        // Frame ids of a plain graphic (frames object keys in JSON order).
        static std::vector<std::string> resolve_plain_frames(
            const Project &project, const std::string &graphic_id);
    };

} // namespace aur
