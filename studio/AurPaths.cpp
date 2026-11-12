#include "AurPaths.hpp"

#include <QDir>

namespace aur
{

    QString dataRoot()
    {
        return QDir::homePath() + "/.Aur";
    }

    QString settingsPath()
    {
        const QString path = dataRoot() + "/settings.ini";
        QDir().mkpath(dataRoot());
        return path;
    }

} // namespace aur
