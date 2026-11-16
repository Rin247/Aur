#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace aur::theme
{

    struct Rgb
    {
        std::uint8_t r = 0;
        std::uint8_t g = 0;
        std::uint8_t b = 0;

        friend bool operator==(const Rgb &a, const Rgb &b)
        {
            return a.r == b.r && a.g == b.g && a.b == b.b;
        }
    };

    constexpr Rgb rgb_from_int(std::uint32_t v)
    {
        return Rgb{static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 8),
                   static_cast<std::uint8_t>(v)};
    }

    bool parse_hex(std::string_view text, Rgb &out);
    std::string to_hex(Rgb c);

    double relative_luminance(Rgb c);
    double contrast_ratio(Rgb a, Rgb b);
    Rgb on_accent(Rgb primary);
    Rgb mix(Rgb from, Rgb toward, double amount);
    Rgb lighter(Rgb c, int percent);
    Rgb darker(Rgb c, int percent);
    bool is_dark_background(Rgb background);

}
