#pragma once

#include <QAudioSink>
#include <QMediaDevices>
#include <atomic>
#include <vector>
#include "aur/audio/RealtimeMixer.hpp"

namespace aur
{
    namespace audio
    {

        // Qt real-time output: pulls frames from the RealtimeMixer inside the sink's
        // IO callback. The callback itself does no locking or allocation beyond what
        // QAudioDevice mandates (contract 4) — the mixer's render() is lock-free
        // apart from the song-swap command boundary.
        class QtAudioOutput : public QObject
        {
            Q_OBJECT
        public:
            explicit QtAudioOutput(QObject *parent = nullptr);
            ~QtAudioOutput() override;

            RealtimeMixer &getMixer() { return mixer; }

            void start(int sampleRate);
            void stop();
            bool isPlaying() const { return sink != nullptr; }

            double levelLeft() const { return mixer.levelLeft.load(std::memory_order_relaxed); }
            double levelRight() const { return mixer.levelRight.load(std::memory_order_relaxed); }

        private:
            RealtimeMixer mixer;
            QAudioSink *sink = nullptr;
            QAudioDevice device;
            std::vector<float> pullBuffer;
        };

    } // namespace audio
} // namespace aur
