#include "PixelFrameOps.hpp"

#include <string>

namespace aur::editor
{

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

} // namespace aur::editor
