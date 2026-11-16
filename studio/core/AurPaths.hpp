#pragma once

// AurPaths.hpp - where Aur keeps its data.
//
// The desktop data root is `~/.Aur`, with the leading dot: a hidden profile
// directory, not a visible one, and not %AppData%, %LOCALAPPDATA% or %TEMP%.
// The settings file lives in it, so the app's state follows the person
// rather than the machine.

#include <QString>

namespace aur
{

    // The data root, `~/.Aur`. It is not created by this call.
    QString dataRoot();

    // The settings file in the data root. The directory is created if it is
    // not there, because QSettings writes an INI file but does not make its
    // directory.
    QString settingsPath();

} // namespace aur
