#include "theme/ThemePalettes.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace aur::theme
{
    namespace
    {
        constexpr Rgb c(std::uint32_t v) { return rgb_from_int(v); }

        // Differentiated dawn/sunrise and dark/midnight palettes synchronized with Hydra.
        constexpr std::array<Mode, 11> kModes{{
            {"brahmamuhurta", "First Light",
             {c(0xeff1f5), c(0xe6e9ef), c(0xdce0e8), c(0xdce0e8), c(0xccd0da), c(0xbcc0cc),
              c(0x4c4f69), c(0x6c6f85), c(0x5a5e78), c(0x2b771b), c(0xc20f36), c(0x915c13)}},
            {"dawn", "Dawn",
             {c(0xfaf0f4), c(0xfdf7fa), c(0xeddce4), c(0xeddce4), c(0xf5e6ed), c(0xd9bfcc),
              c(0x35232d), c(0x6a4e5e), c(0x6f5162), c(0x127637), c(0xb91c1c), c(0xa14d08)}},
            {"sunrise", "Sunrise",
             {c(0xffefd4), c(0xfff9ef), c(0xf5dcb4), c(0xf5dcb4), c(0xfbe8c8), c(0xe0bc84),
              c(0x3e2912), c(0x6e4e28), c(0x75532c), c(0x127637), c(0xb91c1c), c(0x9a4a06)}},
            {"light", "Light",
             {c(0xf4f4f5), c(0xfbfbfd), c(0xe4e4e7), c(0xe4e4e7), c(0xfafafa), c(0xd4d4d8),
              c(0x18181b), c(0x52525b), c(0x5c5c66), c(0x15803d), c(0xb91c1c), c(0xb45309)}},
            {"noon", "Noon",
             {c(0xf7f8fa), c(0xfcfcfd), c(0xe7eaf0), c(0xe7eaf0), c(0xf0f2f6), c(0xd9dee6),
              c(0x1c2028), c(0x4a5160), c(0x5b6272), c(0x15803d), c(0xb91c1c), c(0xb45309)}},
            {"sunset", "Sunset",
             {c(0x22140f), c(0x2d1b14), c(0x3d251b), c(0x362017), c(0x472c20), c(0x613d2b),
              c(0xffe6cf), c(0xd6a98a), c(0xbe8f72), c(0x4caf50), c(0xf44336), c(0xff9800)}},
            {"bluehour", "Blue Hour",
             {c(0x1e1e2e), c(0x242437), c(0x313244), c(0x29293d), c(0x383952), c(0x45475a),
              c(0xcdd6f4), c(0xa6adc8), c(0x969cb8), c(0xa6e3a1), c(0xe78284), c(0xe5c890)}},
            {"dusk", "Dusk",
             {c(0x18131a), c(0x221b25), c(0x312735), c(0x29202d), c(0x3a2f3f), c(0x514157),
              c(0xf5eef8), c(0xc2afc8), c(0xa38fad), c(0x4caf50), c(0xf44336), c(0xff9800)}},
            {"dark", "Dark",
             {c(0x121216), c(0x1a1a20), c(0x27272e), c(0x212128), c(0x303038), c(0x41414b),
              c(0xf4f4f6), c(0xa4a4b0), c(0x9494a0), c(0x4caf50), c(0xf44336), c(0xff9800)}},
            {"midnight", "Midnight",
             {c(0x070a16), c(0x0d1326), c(0x161f3b), c(0x111830), c(0x1d284a), c(0x2d3c66),
              c(0xeef2ff), c(0x99a8cf), c(0x8393bd), c(0x4caf50), c(0xf44336), c(0xff9800)}},
            {"amoled", "Pitch Black",
             {c(0x000000), c(0x0a0a0a), c(0x141414), c(0x181818), c(0x202020), c(0x2c2c2c),
              c(0xfafafa), c(0xa1a1aa), c(0x8b8b96), c(0x4caf50), c(0xf44336), c(0xff9800)}},
        }};

        std::uint8_t clamp_u8(double v)
        {
            return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
        }
    }

    Rgb Tokens::accent_wash() const
    {
        return mix(palette.background, primary, accent_dim_alpha);
    }

    std::span<const Mode> modes()
    {
        return kModes;
    }

    const Mode *find_mode(std::string_view id)
    {
        const std::string_view normalized = normalize_mode_alias(id);
        for (const Mode &mode : kModes)
            if (mode.id == normalized) return &mode;
        return nullptr;
    }

    bool is_valid_mode_id(std::string_view id)
    {
        return id == kSystemMode || id == kCustomMode || find_mode(id) != nullptr;
    }

    std::string_view resolve_mode_id(std::string_view id, bool system_is_dark)
    {
        if (id == kSystemMode) return system_is_dark ? "dark" : "light";
        if (id == kCustomMode) return kCustomMode;
        const Mode *found = find_mode(id);
        return found ? found->id : kDefaultMode;
    }

    Palette derive_custom_palette(Rgb primary)
    {
        const double luma = (0.2126 * primary.r + 0.7152 * primary.g + 0.0722 * primary.b) / 255.0;
        const bool dark = luma < 0.5;
        return Palette{
            mix(dark ? c(0x0f0f1a) : c(0xf4f4f5), primary, 0.10),
            mix(dark ? c(0x18181b) : c(0xfbfbfd), primary, 0.07),
            mix(dark ? c(0x27272a) : c(0xe4e4e7), primary, 0.18),
            mix(dark ? c(0x27272a) : c(0xe4e4e7), primary, 0.18),
            mix(dark ? c(0x1f1f24) : c(0xfafafa), primary, 0.07),
            mix(dark ? c(0x3f3f46) : c(0xd4d4d8), primary, 0.26),
            dark ? c(0xfafafa) : c(0x18181b),
            dark ? c(0xa1a1aa) : c(0x52525b),
            dark ? c(0x9494a0) : c(0x5c5c66),
            dark ? c(0x4caf50) : c(0x15803d),
            dark ? c(0xf44336) : c(0xb91c1c),
            dark ? c(0xff9800) : c(0xb45309),
        };
    }

    Palette resolve_palette(std::string_view id, bool system_is_dark, Rgb primary)
    {
        const std::string_view resolved = resolve_mode_id(id, system_is_dark);
        if (resolved == kCustomMode) return derive_custom_palette(primary);
        const Mode *found = find_mode(resolved);
        return found ? found->palette : find_mode(kDefaultMode)->palette;
    }

    Rgb color_blind_transform(Rgb color, std::string_view mode)
    {
        if (mode.empty() || mode == "none") return color;
        const double r = color.r, g = color.g, b = color.b;
        if (mode == "protanopia")
            return Rgb{clamp_u8(r * 0.567 + g * 0.433), clamp_u8(r * 0.558 + g * 0.442), color.b};
        if (mode == "deuteranopia")
            return Rgb{clamp_u8(r * 0.625 + g * 0.375), clamp_u8(r * 0.7 + g * 0.3), color.b};
        if (mode == "tritanopia")
            return Rgb{color.r, clamp_u8(g * 0.8 + b * 0.2), clamp_u8(g * 0.6 + b * 0.4)};
        if (mode == "achromatopsia")
        {
            const std::uint8_t l = clamp_u8(0.299 * r + 0.587 * g + 0.114 * b);
            return Rgb{l, l, l};
        }
        return color;
    }

    Tokens derive_tokens(const Palette &palette, Rgb primary, std::string_view color_blind_mode)
    {
        Tokens t;
        t.palette = palette;
        t.palette.good = color_blind_transform(palette.good, color_blind_mode);
        t.palette.bad = color_blind_transform(palette.bad, color_blind_mode);
        t.palette.warning = color_blind_transform(palette.warning, color_blind_mode);
        t.primary = primary;
        t.is_dark = is_dark_background(palette.background);
        t.accent_bright = t.is_dark ? lighter(primary, 125) : darker(primary, 115);
        t.on_accent = on_accent(primary);
        t.accent_dim_alpha = t.is_dark ? 0.15 : 0.20;
        return t;
    }

    Choice sanitize_choice(std::string_view mode, std::string_view accent)
    {
        Choice choice{std::string(kDefaultMode), c(kDefaultAccent), {}};
        if (!mode.empty())
        {
            if (is_valid_mode_id(mode)) choice.mode = std::string(normalize_mode_alias(mode));
            else choice.rejected.push_back("theme/mode=" + std::string(mode));
        }
        if (!accent.empty())
        {
            Rgb parsed;
            if (parse_hex(accent, parsed)) choice.accent = parsed;
            else choice.rejected.push_back("theme/accent=" + std::string(accent));
        }
        return choice;
    }

}
