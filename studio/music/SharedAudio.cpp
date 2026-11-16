#include "music/SharedAudio.hpp"

#include "core/AurSettings.hpp"

#include <algorithm>

namespace aur
{

    SharedAudio::SharedAudio(QObject *parent) : QObject(parent), sink(this)
    {
        AurSettings::instance().load();
        percent = std::clamp(AurSettings::instance().masterVolume(), 0, 100);
        sink.getMixer().setMasterVolume(static_cast<float>(percent) / 100.0f);
    }

    void SharedAudio::setMasterVolume(int value)
    {
        const int clamped = std::clamp(value, 0, 100);
        if (clamped == percent)
            return;
        percent = clamped;
        sink.getMixer().setMasterVolume(static_cast<float>(percent) / 100.0f);
        AurSettings::instance().setMasterVolume(percent);
        emit masterVolumeChanged(percent);
    }

} // namespace aur
