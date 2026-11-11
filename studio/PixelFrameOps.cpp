#include "PixelFrameOps.hpp"

#include "aur/gfx.hpp"

#include <string>
#include <utility>
#include <vector>

namespace aur::editor
{

    namespace
    {
        // The anim's frame sequence, or null when the graphic has no anim for
        // this pair. The const path never inserts a key into the document.
        Json *anim_sequence(Json &graphic, const std::string &anim_id)
        {
            if (anim_id.empty()) return nullptr;
            if (!graphic.contains("anims") || !graphic["anims"].is_object()) return nullptr;
            Json &anims = graphic["anims"];
            if (!anims.contains(anim_id)) return nullptr;
            Json &anim = anims[anim_id];
            if (!anim.contains("frames") || !anim["frames"].is_array()) return nullptr;
            return &anim["frames"];
        }

        bool frame_is_used(const Json &graphic, const std::string &frame_id)
        {
            if (!graphic.contains("anims") || !graphic["anims"].is_object()) return false;
            for (const auto &[anim_id, anim] : graphic["anims"].as_object())
            {
                (void)anim_id;
                if (!anim.contains("frames") || !anim["frames"].is_array()) continue;
                for (const Json &frame : anim["frames"].as_array())
                    if (frame.is_string() && frame.as_string() == frame_id)
                        return true;
            }
            return false;
        }
    } // namespace

    JsonArray mirror_rows_right_to_left(const JsonArray &rows)
    {
        JsonArray mirrored;
        mirrored.reserve(rows.size());
        for (const Json &row : rows)
        {
            if (!row.is_string())
            {
                mirrored.emplace_back(row);
                continue;
            }
            const std::string &text = row.as_string();
            const std::size_t width = text.size();
            std::string result = text;
            for (std::size_t x = 0; x < width / 2; ++x)
                result[x] = text[width - 1 - x];
            mirrored.emplace_back(result);
        }
        return mirrored;
    }

    std::string unique_frame_id(const Json &frames, const std::string &base)
    {
        if (base.empty() || !frames.is_object() || !frames.contains(base))
            return base;
        for (int suffix = 2; suffix < 1000; ++suffix)
        {
            const std::string candidate = base + "_" + std::to_string(suffix);
            if (!frames.contains(candidate))
                return candidate;
        }
        return base + "_new";
    }

    bool add_frame(Json &graphic, const std::string &anim_id,
                   const std::string &base_id, std::string &new_id)
    {
        if (!graphic.contains("frames") || !graphic["frames"].is_object()) return false;
        const int width = graphic.contains("w") ? graphic["w"].as_int() : 0;
        const int height = graphic.contains("h") ? graphic["h"].as_int() : 0;
        if (width <= 0 || height <= 0) return false;
        new_id = unique_frame_id(graphic["frames"], base_id.empty() ? "frame" : base_id);
        JsonArray blank;
        for (int y = 0; y < height; ++y)
            blank.emplace_back(std::string(static_cast<std::size_t>(width), '.'));
        graphic["frames"][new_id] = blank;
        if (Json *sequence = anim_sequence(graphic, anim_id))
            sequence->as_array().push_back(new_id);
        return true;
    }

    bool duplicate_frame(Json &graphic, const std::string &anim_id,
                         const std::string &source_id, int source_index,
                         std::string &new_id)
    {
        if (!graphic.contains("frames") || !graphic["frames"].is_object()) return false;
        if (!graphic["frames"].contains(source_id)) return false;
        new_id = unique_frame_id(graphic["frames"], source_id);
        graphic["frames"][new_id] = graphic["frames"][source_id];
        if (Json *sequence = anim_sequence(graphic, anim_id))
        {
            JsonArray &frames = sequence->as_array();
            int at = source_index;
            if (at < 0 || at > static_cast<int>(frames.size())) at = static_cast<int>(frames.size());
            frames.insert(frames.begin() + at, new_id);
        }
        return true;
    }

    bool remove_frame(Json &graphic, const std::string &anim_id,
                      const std::string &frame_id)
    {
        if (!graphic.contains("frames") || !graphic["frames"].is_object()) return false;
        if (!graphic["frames"].contains(frame_id)) return false;
        if (Json *sequence = anim_sequence(graphic, anim_id))
        {
            JsonArray &frames = sequence->as_array();
            for (std::size_t i = 0; i < frames.size(); ++i)
                if (frames[i].is_string() && frames[i].as_string() == frame_id)
                {
                    frames.erase(frames.begin() + static_cast<std::ptrdiff_t>(i));
                    break;
                }
        }
        if (!frame_is_used(graphic, frame_id))
            graphic["frames"].as_object().erase(frame_id);
        return true;
    }

    bool move_frame(Json &graphic, const std::string &anim_id, int index,
                    int delta)
    {
        Json *sequence = anim_sequence(graphic, anim_id);
        if (!sequence) return false;
        JsonArray &frames = sequence->as_array();
        const int target = index + delta;
        if (index < 0 || index >= static_cast<int>(frames.size())) return false;
        if (target < 0 || target >= static_cast<int>(frames.size())) return false;
        std::swap(frames[static_cast<size_t>(index)], frames[static_cast<size_t>(target)]);
        return true;
    }

    JsonArray snap_rgba_to_rows(const std::vector<unsigned char> &rgba, int width,
                                 int height, const std::vector<std::string> &palette)
    {
        JsonArray rows;
        if (width <= 0 || height <= 0 || palette.empty()) return rows;
        // An image that is not there snaps to nothing.
        if (rgba.size() < static_cast<std::size_t>(width) *
                              static_cast<std::size_t>(height) * 4)
            return rows;
        std::vector<Rgb> rgbs;
        rgbs.reserve(palette.size());
        for (const std::string &color : palette)
            rgbs.push_back(hex_to_rgba(color));
        static const char kDigits[] = "0123456789ABCDEF";
        for (int y = 0; y < height; ++y)
        {
            std::string row;
            row.reserve(static_cast<std::size_t>(width));
            for (int x = 0; x < width; ++x)
            {
                const std::size_t offset =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                     static_cast<std::size_t>(x)) * 4;
                if (offset + 3 >= rgba.size())
                    break;
                const int r = rgba[offset];
                const int g = rgba[offset + 1];
                const int b = rgba[offset + 2];
                const int a = rgba[offset + 3];
                int best = 0;
                if (a > 0)
                {
                    int best_distance = -1;
                    for (std::size_t index = 1; index < rgbs.size(); ++index)
                    {
                        const Rgb &color = rgbs[index];
                        const int dr = r - color.r;
                        const int dg = g - color.g;
                        const int db = b - color.b;
                        const int distance = dr * dr + dg * dg + db * db;
                        if (best_distance < 0 || distance < best_distance)
                        {
                            best_distance = distance;
                            best = static_cast<int>(index);
                        }
                    }
                }
                row.push_back(kDigits[best & 0x0f]);
            }
            rows.emplace_back(row);
        }
        return rows;
    }

} // namespace aur::editor
