#include "music/SharedAudio.hpp"

#include <QSettings>

#include <algorithm>

#include "AurPaths.hpp"

namespace aur
{

    SharedAudio::SharedAudio(QObject *parent) : QObject(parent), sink(this)
    {
        QSettings settings(aur::settingsPath(), QSettings::IniFormat);
        percent = std::clamp(settings.value("audio/master_volume", 100).toInt(), 0, 100);
        sink.getMixer().setMasterVolume(static_cast<float>(percent) / 100.0f);
    }

    void SharedAudio::setMasterVolume(int value)
    {
        const int clamped = std::clamp(value, 0, 100);
        if (clamped == percent)
            return;
        percent = clamped;
        sink.getMixer().setMasterVolume(static_cast<float>(percent) / 100.0f);
        QSettings settings(aur::settingsPath(), QSettings::IniFormat);
        settings.setValue("audio/master_volume", percent);
        emit masterVolumeChanged(percent);
    }

} // namespace aur
