#pragma once

#include <QWidget>

#include <QString>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "PixelSurface.hpp"
#include "aur/project.hpp"

namespace aur {

// The pixel editor's canvas: the surface, bound to a graphic's frame. The
// surface knows nothing about graphics; this binding knows which frame is
// being edited and which graphic it belongs to.
class PixelCanvasWidget : public PixelSurface {
    Q_OBJECT
    Project* project = nullptr;
    std::string graphic_id;
    std::string current_frame = "0";

    void bindSurface();

public:
    explicit PixelCanvasWidget(QWidget* parent = nullptr);
    void setGraphic(Project* p, const std::string& id);
    void setFrame(const std::string& frame);
    // The frames the onion skin draws behind the current one.
    void setOnionSkin(const std::vector<std::string>& frames, int current_index);
    // A structural change to the graphic (a frame added, moved or removed),
    // recorded as one undoable snapshot of the whole graphic.
    void applyGraphicChange(const QString& label, const std::function<void(Json&)>& change);
};

} // namespace aur
