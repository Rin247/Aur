#include "PixelEditSession.hpp"

#include <QListWidgetItem>
#include <QPixmap>
#include <QIcon>
#include "aur/gfx.hpp"

#include <set>

namespace aur
{

    namespace
    {
        QColor palette_color(const Json& color) {
            const Rgb rgba = hex_to_rgba(color.as_string());
            return {rgba.r, rgba.g, rgba.b, rgba.a};
        }
    } // namespace

    int PixelEditSession::fill_palette(QListWidget *widget, const Project &project,
                                       const std::string &graphic_id)
    {
        if (!widget)
            return 0;
        widget->clear();
        const auto graphic = project.graphics.find(graphic_id);
        if (graphic == project.graphics.end())
            return 0;
        const Json &json = graphic->second;
        if (!json.contains("palette") || !json["palette"].is_array())
            return 0;
        const auto &palette = json["palette"].as_array();
        for (size_t index = 0; index < palette.size(); ++index)
        {
            const QString color = QString::fromStdString(palette[index].as_string());
            auto *item = new QListWidgetItem(
                QString("%1  %2").arg(static_cast<int>(index), 2, 10, QChar('0')).arg(color),
                widget);
            item->setData(Qt::UserRole, static_cast<int>(index));
            QPixmap swatch(16, 16);
            swatch.fill(palette_color(palette[index]));
            item->setIcon(QIcon(swatch));
        }
        if (widget->count() > 0)
            widget->setCurrentRow(1);
        return static_cast<int>(palette.size());
    }

    std::vector<std::string> PixelEditSession::resolve_anim_frames(
        const Project &project, const std::string &graphic_id,
        const std::string &direction, const std::string &pose)
    {
        std::vector<std::string> frames;
        const auto graphic_it = project.graphics.find(graphic_id);
        if (graphic_it == project.graphics.end())
            return frames;
        const Json &graphic = graphic_it->second;
        const std::string directional_animation = pose + "_" + direction;
        const Json &animations = graphic.contains("anims") ? graphic["anims"] : Json();
        const Json *animation = nullptr;
        if (animations.is_object() && animations.contains(directional_animation))
            animation = &animations[directional_animation];
        else if (animations.is_object() && animations.contains(pose))
            animation = &animations[pose];

        if (animation && (*animation)["frames"].is_array())
        {
            for (const Json &frame : (*animation)["frames"].as_array())
            {
                if (frame.is_string())
                    frames.push_back(frame.as_string());
                else if (frame.is_number())
                    frames.push_back(std::to_string(frame.as_int()));
            }
        }
        if (frames.empty() && graphic["frames"].is_object())
        {
            for (const auto &[frame_id, rows] : graphic["frames"].as_object())
            {
                if (rows.is_array() && (frame_id.find(direction) != std::string::npos ||
                                        direction == "down"))
                    frames.push_back(frame_id);
            }
        }
        std::set<std::string> unique;
        std::vector<std::string> deduplicated;
        for (const std::string &frame : frames)
            if (unique.insert(frame).second)
                deduplicated.push_back(frame);
        return deduplicated;
    }

    std::vector<std::string> PixelEditSession::resolve_plain_frames(
        const Project &project, const std::string &graphic_id)
    {
        std::vector<std::string> frames;
        const auto graphic_it = project.graphics.find(graphic_id);
        if (graphic_it == project.graphics.end())
            return frames;
        const Json &graphic = graphic_it->second;
        if (!graphic.contains("frames") || !graphic["frames"].is_object())
            return frames;
        for (const auto &[frame_id, rows] : graphic["frames"].as_object())
        {
            (void)rows;
            frames.push_back(frame_id);
        }
        return frames;
    }

} // namespace aur
