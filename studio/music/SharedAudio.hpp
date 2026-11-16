#pragma once

// SharedAudio.hpp - the studio's one audio device and its master volume.
//
// The DAW's preview and the playtest's music share it, so the two cannot
// play over each other: the page on screen holds the device, and the other
// one is silent. The master volume is the app's, saved in the data root,
// and both pages move it.

#include <QObject>
#include "music/QtAudioOutput.hpp"

namespace aur
{

    class SharedAudio : public QObject
    {
        Q_OBJECT
    public:
        explicit SharedAudio(QObject *parent = nullptr);

        audio::QtAudioOutput &output() { return sink; }
        audio::RealtimeMixer &mixer() { return sink.getMixer(); }

        // The master volume, 0..100.
        int masterVolume() const { return percent; }
        void setMasterVolume(int value);

    signals:
        void masterVolumeChanged(int percent);

    private:
        audio::QtAudioOutput sink;
        int percent = 100;
    };

} // namespace aur
