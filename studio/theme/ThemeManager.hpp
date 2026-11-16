#pragma once

#include "theme/ThemePalettes.hpp"

#include <QColor>
#include <QObject>
#include <QString>
#include <QStringList>

class QApplication;

namespace aur::theme
{

    class ThemeManager : public QObject
    {
        Q_OBJECT
    public:
        static ThemeManager &instance();

        void load();
        void install(QApplication &app);

        const Tokens &tokens() const { return tokens_; }
        QString modeId() const { return mode_; }
        QColor accent() const;
        int uiScale() const { return ui_scale_; }
        bool dyslexiaFont() const { return dyslexia_font_; }
        QString colorBlindMode() const { return color_blind_mode_; }
        const QStringList &rejected() const { return rejected_; }

        bool setMode(const QString &id);
        bool applyTransientMode(const QString &id);
        bool setAccent(const QColor &color);
        void setUiScale(int percent);
        void setDyslexiaFont(bool enabled);
        bool setColorBlindMode(const QString &mode);

    signals:
        void changed();

    private:
        ThemeManager() = default;
        void rebuild();
        void save() const;

        QString mode_{QString::fromUtf8(kDefaultMode.data(), static_cast<int>(kDefaultMode.size()))};
        Rgb accent_{rgb_from_int(kDefaultAccent)};
        int ui_scale_ = 100;
        bool dyslexia_font_ = false;
        QString color_blind_mode_{QStringLiteral("none")};
        Tokens tokens_{};
        QStringList rejected_;
        bool system_is_dark_ = true;
        bool settings_read_ = false;
        bool installed_ = false;
    };

    inline const Tokens &tokens()
    {
        return ThemeManager::instance().tokens();
    }

}
