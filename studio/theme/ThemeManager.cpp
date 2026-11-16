#include "theme/ThemeManager.hpp"

#include "core/AurSettings.hpp"
#include "theme/AurStyle.hpp"
#include "theme/ThemeQt.hpp"

#include <QApplication>
#include <QFont>
#include <QSettings>
#include <QStyleHints>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace aur::theme
{
    namespace
    {
        bool sample_system_is_dark()
        {
#ifdef Q_OS_WIN
            QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                                         "CurrentVersion\\Themes\\Personalize"),
                          QSettings::NativeFormat);
            const QVariant value = reg.value(QStringLiteral("AppsUseLightTheme"));
            if (value.isValid()) return value.toInt() == 0;
#endif
            return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
        }

        std::string utf8(const QString &s)
        {
            return s.toUtf8().toStdString();
        }

        bool is_valid_color_blind_mode(const QString &m)
        {
            return m == QLatin1String("none") || m == QLatin1String("protanopia") ||
                   m == QLatin1String("deuteranopia") || m == QLatin1String("tritanopia") ||
                   m == QLatin1String("achromatopsia");
        }
    }

    ThemeManager &ThemeManager::instance()
    {
        static ThemeManager manager;
        return manager;
    }

    QColor ThemeManager::accent() const
    {
        return qt(accent_);
    }

    void ThemeManager::load()
    {
        system_is_dark_ = sample_system_is_dark();
        AurSettings &store = AurSettings::instance();
        store.load();
        const Choice choice = sanitize_choice(utf8(store.themeMode()), utf8(store.accentColor()));
        mode_ = QString::fromStdString(choice.mode);
        accent_ = choice.accent;
        ui_scale_ = store.uiScale();
        dyslexia_font_ = store.dyslexiaFont();
        color_blind_mode_ = store.colorBlindMode();
        rejected_.clear();
        for (const QString &key : store.rejectedKeys())
            rejected_ << key;
        settings_read_ = true;
        rebuild();
    }

    void ThemeManager::install(QApplication &app)
    {
        if (!settings_read_) load();
        app.setStyle(new AurStyle);
        installed_ = true;
        rebuild();
    }

    bool ThemeManager::setMode(const QString &id)
    {
        if (!applyTransientMode(id)) return false;
        save();
        return true;
    }

    bool ThemeManager::applyTransientMode(const QString &id)
    {
        const std::string raw = utf8(id);
        if (!is_valid_mode_id(raw)) return false;
        const std::string_view normalized = normalize_mode_alias(raw);
        mode_ = QString::fromUtf8(normalized.data(), static_cast<int>(normalized.size()));
        rebuild();
        return true;
    }

    bool ThemeManager::setAccent(const QColor &color)
    {
        if (!color.isValid()) return false;
        accent_ = Rgb{static_cast<std::uint8_t>(color.red()),
                      static_cast<std::uint8_t>(color.green()),
                      static_cast<std::uint8_t>(color.blue())};
        rebuild();
        save();
        return true;
    }

    void ThemeManager::setUiScale(int percent)
    {
        const int clamped = std::clamp(percent, 75, 200);
        if (ui_scale_ == clamped) return;
        ui_scale_ = clamped;
        rebuild();
        save();
    }

    void ThemeManager::setDyslexiaFont(bool enabled)
    {
        if (dyslexia_font_ == enabled) return;
        dyslexia_font_ = enabled;
        rebuild();
        save();
    }

    bool ThemeManager::setColorBlindMode(const QString &mode)
    {
        if (!is_valid_color_blind_mode(mode)) return false;
        if (color_blind_mode_ == mode) return true;
        color_blind_mode_ = mode;
        rebuild();
        save();
        return true;
    }

    void ThemeManager::rebuild()
    {
        const Palette pal = resolve_palette(utf8(mode_), system_is_dark_, accent_);
        tokens_ = derive_tokens(pal, accent_, utf8(color_blind_mode_));
        if (installed_)
        {
            QFont font;
            if (dyslexia_font_)
                font.setFamilies({QStringLiteral("OpenDyslexic"),
                                  QStringLiteral("Atkinson Hyperlegible"),
                                  QStringLiteral("Segoe UI")});
            else
                font.setFamily(QStringLiteral("Segoe UI"));
            font.setPixelSize(std::max(9, static_cast<int>(std::lround(13.0 * ui_scale_ / 100.0))));
            QApplication::setFont(font);
            QGuiApplication::styleHints()->setColorScheme(tokens_.is_dark ? Qt::ColorScheme::Dark
                                                                           : Qt::ColorScheme::Light);
            QApplication::setPalette(build_palette(tokens_));
        }
        emit changed();
    }

    void ThemeManager::save() const
    {
        if (!settings_read_) return;
        AurSettings &store = AurSettings::instance();
        store.setThemeMode(mode_);
        store.setAccentColor(qt(accent_).name());
        store.setUiScale(ui_scale_);
        store.setDyslexiaFont(dyslexia_font_);
        store.setColorBlindMode(color_blind_mode_);
    }

}
