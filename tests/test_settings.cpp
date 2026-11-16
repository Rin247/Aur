#include "mini_test.hpp"
#include "core/AurSettings.hpp"
#include "core/SettingsSchema.hpp"

#include <cmath>

void test_settings_schema()
{
    aur::ScopedValues scopes;
    scopes.global.insert(QStringLiteral("appearance/themeMode"), QStringLiteral("dark"));
    scopes.global.insert(QStringLiteral("audio/masterVolume"), 80);
    scopes.project.insert(QStringLiteral("audio/masterVolume"), 50);
    scopes.map.insert(QStringLiteral("audio/masterVolume"), 0);

    ASSERT(aur::resolveSetting(QStringLiteral("appearance/themeMode"), scopes).toString() ==
           QLatin1String("dark"));
    // Zero is an explicit override, not absence.
    ASSERT(aur::resolveSetting(QStringLiteral("audio/masterVolume"), scopes).toInt() == 0);
    ASSERT(aur::isOverridden(QStringLiteral("audio/masterVolume"), scopes, true));
    ASSERT(aur::isOverridden(QStringLiteral("audio/masterVolume"), scopes, false));
    ASSERT(!aur::isOverridden(QStringLiteral("appearance/themeMode"), scopes, true));
    ASSERT(!aur::isOverridden(QStringLiteral("appearance/themeMode"), scopes, false));
    ASSERT(aur::resolveSetting(QStringLiteral("missing/key"), scopes, 42).toInt() == 42);
}

void test_settings_validation()
{
    aur::AurSettings store;
    ASSERT(!store.settingsRead());
    ASSERT(store.themeModes().size() == 13);

    bool rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/themeMode"), QStringLiteral("amoled"), &rejected)
               .toString() == QLatin1String("amoled"));
    ASSERT(!rejected);

    rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/themeMode"), QStringLiteral("firstlight"), &rejected)
               .toString() == QLatin1String("brahmamuhurta"));
    ASSERT(!rejected);

    // Legacy ordinal 5 remaps to 7 ("custom").
    rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/themeMode"), QStringLiteral("5"), &rejected)
               .toString() == QLatin1String("custom"));
    ASSERT(!rejected);

    rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/themeMode"), QStringLiteral("nonsense"), &rejected)
               .toString() == QLatin1String("dark"));
    ASSERT(rejected);

    // Accent hex shape check.
    rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/accentColor"), QStringLiteral("#FFD54F"), &rejected)
               .toString() == QLatin1String("#ffd54f"));
    ASSERT(!rejected);

    rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/accentColor"), QStringLiteral("ffd54f"), &rejected)
               .toString() == QLatin1String("#3b82f6"));
    ASSERT(rejected);

    // INI booleans arrive as strings ("true" / "false").
    rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/dyslexiaFont"), QStringLiteral("true"), &rejected)
               .toBool());
    ASSERT(!rejected);

    rejected = false;
    ASSERT(!store.validate(QStringLiteral("audio/enabled"), QStringLiteral("false"), &rejected)
                .toBool());
    ASSERT(!rejected);

    // Real zoom stays a double (1.25 is not truncated to 1).
    rejected = false;
    ASSERT(std::fabs(store.validate(QStringLiteral("appearance/zoom"), 1.25, &rejected).toDouble() - 1.25) < 1e-6);
    ASSERT(!rejected);

    rejected = false;
    ASSERT(std::fabs(store.validate(QStringLiteral("appearance/zoom"), 0.5, &rejected).toDouble() - 1.0) < 1e-6);
    ASSERT(rejected);

    // Integer clamp on uiScale and masterVolume.
    rejected = false;
    ASSERT(store.validate(QStringLiteral("appearance/uiScale"), 300, &rejected).toInt() == 200);
    ASSERT(rejected);

    rejected = false;
    ASSERT(store.validate(QStringLiteral("graphics/backend"), QStringLiteral("vulkan"), &rejected)
               .toString() == QLatin1String("vulkan"));
    ASSERT(!rejected);

    rejected = false;
    ASSERT(store.validate(QStringLiteral("graphics/backend"), QStringLiteral("d3d11"), &rejected)
               .toString() == QLatin1String("d3d12"));
    ASSERT(rejected);
}
