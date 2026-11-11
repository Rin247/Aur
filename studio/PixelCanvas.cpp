#include "PixelCanvas.hpp"

#include <algorithm>

namespace aur
{

    PixelCanvasWidget::PixelCanvasWidget(QWidget* parent) : PixelSurface(parent) {}

    void PixelCanvasWidget::bindSurface() {
        if (!project || !project->graphics.contains(graphic_id)) {
            setRowsTarget(nullptr, {}, 0, 0);
            return;
        }
        Json &graphic = project->graphics[graphic_id];
        Json &frames = graphic["frames"];
        Json *rows = frames.is_object() && frames.contains(current_frame)
                         ? &frames[current_frame] : nullptr;
        std::vector<std::string> colors;
        if (graphic.contains("palette") && graphic["palette"].is_array())
            for (const Json &color : graphic["palette"].as_array())
                colors.push_back(color.is_string() ? color.as_string() : std::string());
        setRowsTarget(rows, colors, graphic["w"].as_int(), graphic["h"].as_int());
    }

    void PixelCanvasWidget::setGraphic(Project* p, const std::string& id) {
        resetStroke();
        if (project != p || graphic_id != id)
            clearHistory();
        project = p;
        graphic_id = id;
        current_frame = "0";
        if (project && project->graphics.contains(graphic_id)) {
            const Json &frames = project->graphics.at(graphic_id)["frames"];
            if (frames.is_object() && !frames.as_object().empty()) {
                current_frame = frames.contains(current_frame)
                    ? current_frame : frames.as_object().begin()->first;
            }
        }
        bindSurface();
        update();
    }

    void PixelCanvasWidget::setFrame(const std::string& frame) {
        if (!project || !project->graphics.contains(graphic_id)) return;
        const Json &frames = project->graphics.at(graphic_id)["frames"];
        if (!frames.is_object() || !frames.contains(frame) || !frames[frame].is_array()) return;
        // History is per-frame now: switching frames (including the animation
        // preview's setFrame ticks) no longer wipes the undo stack.
        current_frame = frame;
        bindSurface();
        update();
    }

    void PixelCanvasWidget::setOnionSkin(const std::vector<std::string>& frames,
                                         int current_index) {
        std::vector<const Json*> rows;
        if (project && project->graphics.contains(graphic_id)) {
            Json &graphic = project->graphics[graphic_id];
            Json &frame_map = graphic["frames"];
            for (const std::string &frame : frames)
                if (frame_map.is_object() && frame_map.contains(frame))
                    rows.push_back(&frame_map[frame]);
        }
        PixelSurface::setOnionSkin(rows, current_index);
    }

    void PixelCanvasWidget::applyGraphicChange(const QString& label,
                                               const std::function<void(Json&)>& change) {
        if (!project || !project->graphics.contains(graphic_id)) return;
        applyChange(&project->graphics[graphic_id], label, change);
    }

} // namespace aur
