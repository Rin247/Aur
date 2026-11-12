#pragma once

// TransportIcon.hpp - the DAW transport's glyphs.
//
// The style's own media icons are drawn for a light theme, and a black
// play triangle on a dark button cannot be seen, so the three glyphs are
// drawn here in the app's own colour.

#include <QIcon>

namespace aur
{
    namespace music
    {

        enum class TransportGlyph { Play, Pause, Stop };

        QIcon transportIcon(TransportGlyph glyph);

    } // namespace music
} // namespace aur
