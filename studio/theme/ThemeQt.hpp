#pragma once

#include "theme/ThemePalettes.hpp"

#include <QColor>
#include <QPalette>

namespace aur::theme
{

    inline QColor qt(Rgb c)
    {
        return QColor(c.r, c.g, c.b);
    }

    QPalette build_palette(const Tokens &tokens);

}
