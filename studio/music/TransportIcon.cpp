#include "TransportIcon.hpp"

#include "theme/ThemeIcons.hpp"

namespace aur
{
    namespace music
    {

        QIcon transportIcon(TransportGlyph glyph)
        {
            switch (glyph)
            {
            case TransportGlyph::Play: return theme::icon(theme::Glyph::Play);
            case TransportGlyph::Pause: return theme::icon(theme::Glyph::Pause);
            case TransportGlyph::Stop: return theme::icon(theme::Glyph::Stop);
            }
            return theme::icon(theme::Glyph::Play);
        }

    } // namespace music
} // namespace aur
