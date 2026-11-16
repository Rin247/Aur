#include "theme/ThemeQt.hpp"

namespace aur::theme
{

    QPalette build_palette(const Tokens &tokens)
    {
        const Palette &p = tokens.palette;
        const Rgb disabled_text = mix(p.text_dim, p.background, 0.35);

        QPalette pal;
        pal.setColor(QPalette::Window, qt(p.background));
        pal.setColor(QPalette::Base, qt(p.surface));
        pal.setColor(QPalette::AlternateBase, qt(p.surface_variant));
        pal.setColor(QPalette::WindowText, qt(p.text));
        pal.setColor(QPalette::Text, qt(p.text));
        pal.setColor(QPalette::ButtonText, qt(p.text));
        pal.setColor(QPalette::Button, qt(p.surface_secondary));
        pal.setColor(QPalette::Light, qt(p.surface_high));
        pal.setColor(QPalette::Midlight, qt(p.surface_high));
        pal.setColor(QPalette::Mid, qt(p.divider));
        pal.setColor(QPalette::Dark, qt(p.divider));
        pal.setColor(QPalette::Shadow, qt(p.divider));
        pal.setColor(QPalette::Highlight, qt(tokens.primary));
        pal.setColor(QPalette::HighlightedText, qt(tokens.on_accent));
        pal.setColor(QPalette::Link, qt(tokens.primary));
        pal.setColor(QPalette::LinkVisited, qt(tokens.primary));
        pal.setColor(QPalette::BrightText, qt(tokens.on_accent));
        pal.setColor(QPalette::ToolTipBase, qt(p.surface_high));
        pal.setColor(QPalette::ToolTipText, qt(p.text));
        pal.setColor(QPalette::PlaceholderText, qt(p.text_dim));

        for (QPalette::ColorRole role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
            pal.setColor(QPalette::Disabled, role, qt(disabled_text));
        pal.setColor(QPalette::Disabled, QPalette::Button, qt(p.surface));
        pal.setColor(QPalette::Disabled, QPalette::Highlight, qt(p.divider));
        pal.setColor(QPalette::Disabled, QPalette::HighlightedText, qt(disabled_text));
        return pal;
    }

}
