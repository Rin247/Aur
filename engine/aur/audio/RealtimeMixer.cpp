#include "aur/audio/RealtimeMixer.hpp"

#include <algorithm>
#include <cmath>

namespace aur
{
    namespace audio
    {
        namespace
        {

            constexpr double pi = 3.14159265358979323846;

            Timbre timbreForSong(const Json &song)
            {
                const std::string synthId = song.contains("preview_synth") &&
                                                    song["preview_synth"].is_string()
                                                ? song["preview_synth"].as_string()
                                                : "gm";
                return synthId == "retro_psg" ? Timbre::RetroPsg
                       : synthId == "fm"      ? Timbre::Fm
                                              : Timbre::Gm;
            }

            int clampedInt(const Json &value, int fallback, int low, int high)
            {
                return value.is_number() ? std::clamp(value.as_int(), low, high) : fallback;
            }

        } // namespace

        void RealtimeMixer::loadSong(const Json &song)
        {
            std::lock_guard<std::mutex> lock(songMutex);
            pendingSong = song;
            pending = true;
        }

        void RealtimeMixer::audition(int pitch, int velocity, int lengthSteps)
        {
            Voice voice;
            voice.pitch = std::clamp(pitch, 0, 127);
            voice.frequency = 440.0 * std::pow(2.0, (voice.pitch - 69) / 12.0);
            voice.amplitude = 0.14 * std::clamp(velocity, 0, 127) / 127.0;
            voice.pan = 0.5;
            voice.program = 0;
            voice.drum = false;
            std::lock_guard<std::mutex> lock(songMutex);
            // start/end resolved against frameCounter at the command boundary;
            // store the requested length in steps via end-start for now.
            voice.start = 0;
            voice.end = std::max(1, lengthSteps);
            pendingAuditions.push_back(voice);
        }

        void RealtimeMixer::schedule(const Json &song, std::int64_t baseFrame)
        {
            voices.clear();
            upcoming.clear();
            loopVoices.clear();
            bpm = clampedInt(song.contains("tempo") ? Json(song["tempo"]) : Json{},
                             120, 30, 240);
            const double framesPerStep = sampleRate * 60.0 / (bpm * 4.0);
            int lastStep = song.contains("bars") && song["bars"].is_number()
                               ? std::max(song["bars"].as_int() * 16, 16)
                               : 128;
            loopSteps = lastStep;

            timbre = timbreForSong(song);
            if (!song.contains("tracks") || !song["tracks"].is_array())
                return;
            for (const Json &track : song["tracks"].as_array())
            {
                if (!track.contains("notes") || !track["notes"].is_array())
                    continue;
                const int trackVolume = clampedInt(
                    track.contains("volume") ? Json(track["volume"]) : Json{}, 90, 0, 127);
                const int panValue = clampedInt(
                    track.contains("pan") ? Json(track["pan"]) : Json{}, 64, 0, 127);
                for (const Json &note : track["notes"].as_array())
                {
                    if (!note.is_array() || note.as_array().size() < 3)
                        continue;
                    const int step = std::max(note[0].as_int(), 0);
                    const int length = std::clamp(note[2].as_int(), 1, 512);
                    const int velocity = note.as_array().size() >= 4
                                             ? std::clamp(note[3].as_int(), 0, 127)
                                             : 90;
                    Voice voice;
                    voice.start = baseFrame + static_cast<std::int64_t>(step * framesPerStep);
                    voice.end = voice.start + static_cast<std::int64_t>(length * framesPerStep);
                    voice.pitch = std::clamp(note[1].as_int(), 0, 127);
                    voice.frequency = 440.0 * std::pow(2.0, (voice.pitch - 69) / 12.0);
                    voice.amplitude = 0.14 * trackVolume / 127.0 * velocity / 127.0;
                    voice.pan = panValue / 127.0;
                    voice.program = clampedInt(
                        track.contains("program") ? Json(track["program"]) : Json{}, 0, 0, 127);
                    voice.drum = (track.contains("channel") && track["channel"].is_number() &&
                                  track["channel"].as_int() == 9) ||
                                 (track.contains("drum") && track["drum"].is_bool() &&
                                  track["drum"].as_bool());
                    upcoming.push_back(voice);
                    lastStep = std::max(lastStep, step + length);
                }
            }
            loopSteps = lastStep;
            nextWrapFrame = baseFrame +
                static_cast<std::int64_t>(lastStep * framesPerStep);
            loopVoices = std::vector<Voice>(upcoming.begin(), upcoming.end());
        }

        void RealtimeMixer::render(float *out, int frames)
        {
            // Command boundary: swap in a queued song outside the sample loop. The
            // mutex is taken here, not per-sample.
            if (pending)
            {
                Json song;
                {
                    std::lock_guard<std::mutex> lock(songMutex);
                    song = std::move(pendingSong);
                    pending = false;
                }
                schedule(song, frameCounter);
            }
            if (!pendingAuditions.empty())
            {
                const double auditionFramesPerStep = sampleRate * 60.0 / (bpm * 4.0);
                for (Voice &a : pendingAuditions)
                {
                    Voice v = a;
                    const std::int64_t length =
                        static_cast<std::int64_t>(v.end * auditionFramesPerStep);
                    v.start = frameCounter + frames; // next callback
                    v.end = v.start + length;
                    upcoming.push_back(v);
                }
                pendingAuditions.clear();
            }

            const double framesPerStep = sampleRate * 60.0 / (bpm * 4.0);
            const std::int64_t loopFrames =
                loopSteps > 0 ? static_cast<std::int64_t>(loopSteps * framesPerStep) : 0;

            // Activate voices whose start has arrived (upcoming kept sorted at build).
            for (auto it = upcoming.begin(); it != upcoming.end() &&
                                             it->start <= frameCounter + frames;)
            {
                voices.push_back(*it);
                it = upcoming.erase(it);
            }

            double peakLeft = 0.0;
            double peakRight = 0.0;
            for (int i = 0; i < frames; ++i, ++frameCounter)
            {
                // Loop wrap: schedule the next pass at its absolute offset;
                // frameCounter is monotonically increasing.
                if (loopFrames > 0 && frameCounter >= nextWrapFrame)
                {
                    for (const Voice &tmpl : loopVoices)
                    {
                        Voice v = tmpl;
                        v.start += nextWrapFrame;
                        v.end += nextWrapFrame;
                        upcoming.push_back(v);
                    }
                    nextWrapFrame += loopFrames;
                    for (auto it = upcoming.begin();
                         it != upcoming.end() && it->start <= frameCounter + 1;)
                    {
                        voices.push_back(*it);
                        it = upcoming.erase(it);
                    }
                }

                double left = 0.0;
                double right = 0.0;
                for (auto it = voices.begin(); it != voices.end();)
                {
                    if (frameCounter >= it->end)
                    {
                        it = voices.erase(it);
                        continue;
                    }
                    if (frameCounter < it->start)
                    {
                        ++it; // scheduled ahead; keep for its start frame
                        continue;
                    }
                    const double time = static_cast<double>(frameCounter - it->start) /
                                        sampleRate;
                    const double value = render_voice_sample(
                        *it, timbre, time, frameCounter, sampleRate);
                    const double gain = it->amplitude *
                                        voice_envelope(*it, frameCounter, sampleRate);
                    left += value * gain * std::cos(it->pan * pi / 2.0);
                    right += value * gain * std::sin(it->pan * pi / 2.0);
                    ++it;
                }
                const float l = static_cast<float>(std::clamp(left, -0.95, 0.95));
                const float r = static_cast<float>(std::clamp(right, -0.95, 0.95));
                out[2 * i] = l;
                out[2 * i + 1] = r;
                peakLeft = std::max(peakLeft, std::abs(static_cast<double>(l)));
                peakRight = std::max(peakRight, std::abs(static_cast<double>(r)));
            }
            levelLeft.store(peakLeft, std::memory_order_relaxed);
            levelRight.store(peakRight, std::memory_order_relaxed);
            playheadFrames.store(frameCounter, std::memory_order_relaxed);
            playheadBpm.store(bpm, std::memory_order_relaxed);
            playheadActive.store(true, std::memory_order_relaxed);
        }

    } // namespace audio
} // namespace aur
