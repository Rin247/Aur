#pragma once

#include <QIcon>

namespace aur::theme
{

    enum class Glyph
    {
        Hamburger,
        Minimize,
        Maximize,
        Restore,
        Close,
        Save,
        Open,
        Reload,
        Play,
        Pause,
        Stop,
        Appearance
    };

    QIcon icon(Glyph glyph);

}
