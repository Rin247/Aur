#pragma once

#include "theme/ThemeMath.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace aur::theme
{

    struct Palette
    {
        Rgb background;
        Rgb surface;
        Rgb surface_variant;
        Rgb surface_secondary;
        Rgb surface_high;
        Rgb divider;
        Rgb text;
        Rgb text_dim;
        Rgb text_mute;
        Rgb good;
        Rgb bad;
        Rgb warning;
    };

    struct Mode
    {
        std::string_view id;
        std::string_view name;
        Palette palette;
    };

    struct Tokens
    {
        Palette palette;
        Rgb primary;
        Rgb accent_bright;
        Rgb on_accent;
        double accent_dim_alpha = 0.15;
        bool is_dark = true;

        Rgb accent_wash() const;
    };

    struct Choice
    {
        std::string mode;
        Rgb accent;
        std::vector<std::string> rejected;
    };

    inline constexpr std::string_view kSystemMode = "system";
    inline constexpr std::string_view kDefaultMode = "dark";
    inline constexpr std::uint32_t kDefaultAccent = 0x3b82f6;

    std::span<const Mode> modes();
    const Mode *find_mode(std::string_view id);
    bool is_valid_mode_id(std::string_view id);
    std::string_view resolve_mode_id(std::string_view id, bool system_is_dark);
    Tokens derive_tokens(const Palette &palette, Rgb primary);
    Choice sanitize_choice(std::string_view mode, std::string_view accent);

}
