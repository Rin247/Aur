#include "theme/ThemeManager.hpp"

#include "AurPaths.hpp"
#include "theme/AurStyle.hpp"
#include "theme/ThemeQt.hpp"

#include <QApplication>
#include <QFont>
#include <QSettings>
#include <QStyleHints>
#include <QtGlobal>

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
        QSettings settings(aur::settingsPath(), QSettings::IniFormat);
        const Choice choice = sanitize_choice(
            utf8(settings.value(QStringLiteral("theme/mode")).toString()),
            utf8(settings.value(QStringLiteral("theme/accent")).toString()));
        mode_ = QString::fromStdString(choice.mode);
        accent_ = choice.accent;
        rejected_.clear();
        for (const std::string &item : choice.rejected)
        {
            rejected_ << QString::fromStdString(item);
            qWarning("Aur: rejected setting %s", item.c_str());
        }
        settings_read_ = true;
        rebuild();
        if (!rejected_.isEmpty()) save();
    }

    void ThemeManager::install(QApplication &app)
    {
        if (!settings_read_) load();
        app.setStyle(new AurStyle);
        QFont font(QStringLiteral("Segoe UI"));
        font.setPixelSize(13);
        app.setFont(font);
        installed_ = true;
        rebuild();
    }

    bool ThemeManager::setMode(const QString &id)
    {
        if (!is_valid_mode_id(utf8(id))) return false;
        mode_ = id;
        rebuild();
        save();
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

    void ThemeManager::rebuild()
    {
        const std::string id = utf8(mode_);
        const Mode *mode = find_mode(resolve_mode_id(id, system_is_dark_));
        tokens_ = derive_tokens(mode->palette, accent_);
        if (installed_)
        {
            QGuiApplication::styleHints()->setColorScheme(tokens_.is_dark ? Qt::ColorScheme::Dark
                                                                           : Qt::ColorScheme::Light);
            QApplication::setPalette(build_palette(tokens_));
        }
        emit changed();
    }

    void ThemeManager::save() const
    {
        if (!settings_read_) return;
        QSettings settings(aur::settingsPath(), QSettings::IniFormat);
        settings.setValue(QStringLiteral("theme/mode"), mode_);
        settings.setValue(QStringLiteral("theme/accent"), qt(accent_).name());
    }

}
