#pragma once

#include <QByteArray>
#include <QString>
#include "aur/audio/synth.hpp"
#include "aur/json.hpp"

namespace aur
{
    namespace music
    {

        Json makeTrack(const std::string &name, int channel, int program);
        JsonArray notesForBar(const Json &track, int bar);
        void initializeScenes(Json &song);
        std::string songIdFromName(const QString &name);

        Json quickSong(const std::string &style);

        QByteArray stereoPcmToWav(const class StereoPcm &pcm);

    } // namespace music
} // namespace aur
