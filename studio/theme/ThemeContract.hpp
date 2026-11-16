#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

namespace aur::theme
{

    // Carried over from Hydra/src/ThemeContract.h: 12 picker modes in 24-hour clock
    // order, plus "custom" (accent-tinted neutral ramp) at index 12.
    inline constexpr const char *kThemeModes[] = {
        "system", "brahmamuhurta", "dawn", "sunrise", "light", "noon",
        "sunset", "bluehour", "dusk", "dark", "midnight", "amoled", "custom"};
    inline constexpr std::size_t kThemeModeCount = sizeof(kThemeModes) / sizeof(kThemeModes[0]);

    inline constexpr int kLegacyCustomThemeOrdinal = 5;

    struct LegacyTheme
    {
        int ordinal;
        const char *modeId;
    };

    inline constexpr LegacyTheme kLegacyThemes[] = {
        {0, "system"},
        {1, "light"},
        {2, "midnight"},
        {3, "dark"},
        {4, "amoled"},
        {5, "fog"},
        {6, "noon"},
        {7, "custom"},
    };
    inline constexpr std::size_t kLegacyThemeCount =
        sizeof(kLegacyThemes) / sizeof(kLegacyThemes[0]);

    inline constexpr int resolveLegacyThemeOrdinal(int ordinal)
    {
        return (ordinal == kLegacyCustomThemeOrdinal) ? 7 : ordinal;
    }

    inline constexpr const char *legacyModeIdFor(int ordinal)
    {
        const int resolved = resolveLegacyThemeOrdinal(ordinal);
        for (std::size_t i = 0; i < kLegacyThemeCount; ++i)
        {
            if (kLegacyThemes[i].ordinal == resolved)
                return kLegacyThemes[i].modeId;
        }
        return nullptr;
    }

    inline constexpr std::string_view normalize_mode_alias(std::string_view id)
    {
        if (id == "firstlight") return "brahmamuhurta";
        return id;
    }

}
