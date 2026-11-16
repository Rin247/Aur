#include "mini_test.hpp"
#include "theme/ThemeQt.hpp"

#include <cmath>
#include <cstring>
#include <set>
#include <string>

namespace
{
    using namespace aur::theme;

    Rgb hex(const char *text)
    {
        Rgb out;
        parse_hex(text, out);
        return out;
    }

    bool near(double a, double b, double tolerance = 0.02)
    {
        return std::fabs(a - b) <= tolerance;
    }

    const Palette &palette_of(const char *id)
    {
        return find_mode(id)->palette;
    }
}

void test_theme_math()
{
    Rgb out;
    ASSERT(parse_hex("#3b82f6", out));
    ASSERT(to_hex(out) == "#3b82f6");
    ASSERT(parse_hex("#3B82F6", out));
    ASSERT(!parse_hex("3b82f6", out));
    ASSERT(!parse_hex("#3b82f", out));
    ASSERT(!parse_hex("#3b82f6ff", out));
    ASSERT(!parse_hex("#gggggg", out));
    ASSERT(!parse_hex("", out));

    ASSERT(near(relative_luminance(hex("#0f0f1a")), 0.005178, 0.0001));
    ASSERT(near(relative_luminance(hex("#3b82f6")), 0.2355, 0.001));
    ASSERT(near(relative_luminance(hex("#ffffff")), 1.0, 0.0001));

    ASSERT(on_accent(hex("#0f0f0f")) == hex("#ffffff"));
    ASSERT(on_accent(hex("#787878")) == hex("#ffffff"));
    ASSERT(on_accent(hex("#808080")) == hex("#0f0f1a"));
    ASSERT(on_accent(hex("#3b82f6")) == hex("#0f0f1a"));
    ASSERT(on_accent(hex("#ffd54f")) == hex("#0f0f1a"));
    ASSERT(on_accent(hex("#ffffff")) == hex("#0f0f1a"));

    ASSERT(mix(hex("#000000"), hex("#ffffff"), 0.5) == hex("#808080"));
    ASSERT(mix(hex("#102030"), hex("#ffffff"), 0.0) == hex("#102030"));
    ASSERT(lighter(hex("#404040"), 200) == hex("#808080"));
    ASSERT(darker(hex("#808080"), 200) == hex("#404040"));
}

void test_theme_qpalette()
{
    for (const Mode &mode : modes())
    {
        const Tokens t = derive_tokens(mode.palette, hex("#3b82f6"));
        const QPalette pal = build_palette(t);
        ASSERT(pal.color(QPalette::Window) == qt(mode.palette.background));
        ASSERT(pal.color(QPalette::Base) == qt(mode.palette.surface));
        ASSERT(pal.color(QPalette::Button) == qt(mode.palette.surface_secondary));
        ASSERT(pal.color(QPalette::Mid) == qt(mode.palette.divider));
        ASSERT(pal.color(QPalette::Highlight) == qt(t.primary));
        ASSERT(pal.color(QPalette::HighlightedText) == qt(t.on_accent));
        ASSERT(pal.color(QPalette::PlaceholderText) == qt(mode.palette.text_dim));
        ASSERT(pal.color(QPalette::Disabled, QPalette::Text) != pal.color(QPalette::Active, QPalette::Text));
        ASSERT(pal.color(QPalette::Disabled, QPalette::Text) != qt(mode.palette.background));
    }
}

void test_theme_palettes()
{
    ASSERT(kThemeModeCount == 13);
    ASSERT(std::strcmp(kThemeModes[0], "system") == 0);
    ASSERT(std::strcmp(kThemeModes[1], "brahmamuhurta") == 0);
    ASSERT(std::strcmp(kThemeModes[12], "custom") == 0);
    ASSERT(kLegacyCustomThemeOrdinal == 5);
    ASSERT(resolveLegacyThemeOrdinal(5) == 7);
    ASSERT(std::strcmp(legacyModeIdFor(0), "system") == 0);
    ASSERT(std::strcmp(legacyModeIdFor(1), "light") == 0);
    ASSERT(std::strcmp(legacyModeIdFor(2), "midnight") == 0);
    ASSERT(std::strcmp(legacyModeIdFor(3), "dark") == 0);
    ASSERT(std::strcmp(legacyModeIdFor(4), "amoled") == 0);
    ASSERT(std::strcmp(legacyModeIdFor(5), "custom") == 0);
    ASSERT(std::strcmp(legacyModeIdFor(6), "noon") == 0);
    ASSERT(std::strcmp(legacyModeIdFor(7), "custom") == 0);
    ASSERT(legacyModeIdFor(-1) == nullptr);
    ASSERT(legacyModeIdFor(99) == nullptr);

    ASSERT(modes().size() == 11);
    std::set<std::string> ids;
    for (const Mode &mode : modes()) ids.insert(std::string(mode.id));
    ASSERT(ids.size() == 11);
    ASSERT(find_mode("firstlight") == find_mode("brahmamuhurta"));

    const std::set<std::string> light{"brahmamuhurta", "dawn", "sunrise", "light", "noon"};
    int light_count = 0;
    for (const Mode &mode : modes())
    {
        const double l = relative_luminance(mode.palette.background);
        const bool expect_light = light.count(std::string(mode.id)) > 0;
        ASSERT(is_dark_background(mode.palette.background) == !expect_light);
        if (expect_light)
        {
            ++light_count;
            ASSERT(l > 0.87 && l < 0.95);
        }
        else
        {
            ASSERT(l < 0.015);
        }
        ASSERT(contrast_ratio(mode.palette.text, mode.palette.background) >= 7.0);
    }
    ASSERT(light_count == 5);

    ASSERT(near(contrast_ratio(palette_of("dark").text, palette_of("dark").background), 17.01));
    ASSERT(near(contrast_ratio(palette_of("brahmamuhurta").text, palette_of("brahmamuhurta").background), 7.06));
    ASSERT(near(contrast_ratio(palette_of("amoled").text_dim, palette_of("amoled").background), 8.19));
    ASSERT(near(contrast_ratio(palette_of("light").good, palette_of("light").background), 4.56));
    ASSERT(near(contrast_ratio(palette_of("amoled").text_mute, palette_of("amoled").surface_variant), 5.47));
    ASSERT(near(contrast_ratio(palette_of("midnight").text_mute, palette_of("midnight").surface_variant), 5.31));

    for (const Mode &mode : modes())
    {
        const Palette &p = mode.palette;
        ASSERT(contrast_ratio(p.text, p.surface) >= 4.5);
        ASSERT(contrast_ratio(p.text_dim, p.background) >= 4.3);
    }

    ASSERT(is_valid_mode_id("system"));
    ASSERT(is_valid_mode_id("brahmamuhurta"));
    ASSERT(is_valid_mode_id("firstlight"));
    ASSERT(is_valid_mode_id("amoled"));
    ASSERT(is_valid_mode_id("custom"));
    ASSERT(!is_valid_mode_id("fog"));
    ASSERT(!is_valid_mode_id("Dark"));
    ASSERT(resolve_mode_id("system", true) == "dark");
    ASSERT(resolve_mode_id("system", false) == "light");
    ASSERT(resolve_mode_id("firstlight", true) == "brahmamuhurta");
    ASSERT(resolve_mode_id("custom", false) == "custom");
    ASSERT(resolve_mode_id("noon", true) == "noon");
    ASSERT(resolve_mode_id("bogus", false) == "dark");

    const Palette customDark = derive_custom_palette(hex("#3b82f6"));
    ASSERT(is_dark_background(customDark.background));
    ASSERT(customDark.text == hex("#fafafa"));
    const Palette customLight = derive_custom_palette(hex("#ffd54f"));
    ASSERT(!is_dark_background(customLight.background));
    ASSERT(customLight.text == hex("#18181b"));

    const Choice fresh = sanitize_choice("", "");
    ASSERT(fresh.mode == "dark" && fresh.accent == hex("#3b82f6") && fresh.rejected.empty());
    const Choice good = sanitize_choice("custom", "#ffd54f");
    ASSERT(good.mode == "custom" && good.accent == hex("#ffd54f") && good.rejected.empty());
    const Choice alias = sanitize_choice("firstlight", "#3b82f6");
    ASSERT(alias.mode == "brahmamuhurta" && alias.rejected.empty());
    const Choice bad = sanitize_choice("fog", "3b82f6");
    ASSERT(bad.mode == "dark" && bad.accent == hex("#3b82f6") && bad.rejected.size() == 2);

    const Rgb achrom = color_blind_transform(hex("#4caf50"), "achromatopsia");
    ASSERT(achrom.r == achrom.g && achrom.g == achrom.b);
    const Tokens cbTokens = derive_tokens(palette_of("dark"), hex("#3b82f6"), "protanopia");
    ASSERT(cbTokens.palette.good != palette_of("dark").good);

    const Tokens dark = derive_tokens(palette_of("dark"), hex("#3b82f6"));
    ASSERT(dark.is_dark && near(dark.accent_dim_alpha, 0.15));
    ASSERT(relative_luminance(dark.accent_bright) > relative_luminance(dark.primary));
    ASSERT(dark.on_accent == hex("#0f0f1a"));
    const Tokens pale = derive_tokens(palette_of("light"), hex("#3b82f6"));
    ASSERT(!pale.is_dark && near(pale.accent_dim_alpha, 0.20));
    ASSERT(relative_luminance(pale.accent_bright) < relative_luminance(pale.primary));
}
