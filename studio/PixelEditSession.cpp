#include "PixelEditSession.hpp"

#include <QListWidgetItem>
#include <QPixmap>
#include <QIcon>
#include "aur/gfx.hpp"

#include <cctype>
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

    PixelEditSession::Anim PixelEditSession::resolve_anim(
        const Project &project, const std::string &graphic_id,
        const std::string &direction, const std::string &pose)
    {
        Anim anim;
        const auto graphic_it = project.graphics.find(graphic_id);
        if (graphic_it == project.graphics.end())
            return anim;
        const Json &graphic = graphic_it->second;
        const Json &animations = graphic.contains("anims") ? graphic["anims"] : Json();
        if (!animations.is_object())
            return anim;
        const std::string directional_animation = pose + "_" + direction;
        if (animations.contains(directional_animation))
            anim.id = directional_animation;
        else if (animations.contains(pose))
            anim.id = pose;
        if (anim.id.empty())
            return anim;
        const Json &animation = animations[anim.id];
        if (animation.is_object())
        {
            if (animation.contains("fps") && animation["fps"].is_number())
                anim.fps = animation["fps"].as_int();
            if (animation.contains("loop") && animation["loop"].is_bool())
                anim.loop = animation["loop"].as_bool();
        }
        return anim;
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
        const Anim anim = resolve_anim(project, graphic_id, direction, pose);
        if (!anim.id.empty())
        {
            const Json &animation = graphic["anims"][anim.id];
            if (animation["frames"].is_array())
            {
                for (const Json &frame : animation["frames"].as_array())
                {
                    if (frame.is_string())
                        frames.push_back(frame.as_string());
                    else if (frame.is_number())
                        frames.push_back(std::to_string(frame.as_int()));
                }
            }
        }
        if (frames.empty() && graphic["frames"].is_object())
        {
            // Whole-direction-token fallback (editsweep D1.S4): "right" must
            // not match "up_right"/"down_right". "down" still lists all
            // frames (legacy catch-all for undirected sheets).
            const bool catch_all = direction == "down";
            static const char* kDirections[] = {
                "down", "down_right", "down_left", "up", "up_right",
                "up_left", "right", "left"};
            auto is_direction = [](const std::string& word) {
                for (const char* d : kDirections)
                    if (word == d) return true;
                return false;
            };
            auto token_match = [&](const std::string& frame_id)
            {
                size_t pos = 0;
                while ((pos = frame_id.find(direction, pos)) != std::string::npos)
                {
                    // A token starts the id or follows '_' (e.g. "right_0"); a
                    // '_' before it means a compound direction ("up_right")
                    // that must not match "right", and a direction word after
                    // it ("up_right") must not match "up".
                    const bool left_ok = pos == 0 || frame_id[pos - 1] != '_';
                    const size_t end = pos + direction.size();
                    bool right_ok = true;
                    if (end < frame_id.size() && frame_id[end] == '_')
                    {
                        const size_t word_start = end + 1;
                        const size_t word_end =
                            std::min(frame_id.find('_', word_start), frame_id.size());
                        if (is_direction(frame_id.substr(word_start, word_end - word_start)))
                            right_ok = false;
                    }
                    if (left_ok && right_ok) return true;
                    pos = end;
                }
                return false;
            };
            for (const auto &[frame_id, rows] : graphic["frames"].as_object())
            {
                if (rows.is_array() && (catch_all || token_match(frame_id)))
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
