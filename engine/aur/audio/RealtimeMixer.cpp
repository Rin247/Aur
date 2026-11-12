#include "aur/audio/RealtimeMixer.hpp"

#include "aur/audio/SongDocument.hpp"

#include "aur/audio/StepSequencer.hpp"

#include "aur/audio/SynthCores.hpp"

#include <algorithm>

#include <cmath>

#include <limits>

namespace aur
{
    namespace audio
    {
        namespace
        {

            // The song's preview_synth id picks the core. The retired oscillator
            // ids ("gm", "fm", "retro_psg") fall through to the YM-FM core.
            SynthCoreVoice::Kind coreKindForSong(const Json &song)
            {
                const std::string synthId = song.contains("preview_synth") &&
                                                    song["preview_synth"].is_string()
                                                ? song["preview_synth"].as_string()
                                                : "md_fm";
                return synthId == "spc" ? SynthCoreVoice::Kind::Spc
                                        : SynthCoreVoice::Kind::MdFm;
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
            CoreNoteEvent event;
            event.pitch = std::clamp(pitch, 0, 127);
            event.velocity = std::clamp(velocity, 1, 127);
            event.pan = 64;
            event.program = 0;
            event.track = 0;
            event.startFrame = 0;
            event.endFrame = std::max(1, lengthSteps);
            std::lock_guard<std::mutex> lock(songMutex);
            pendingAuditions.push_back(event);
        }

        void RealtimeMixer::schedule(const Json &song, std::int64_t baseFrame)
        {
            bpm = clampedInt(song.contains("tempo") ? Json(song["tempo"]) : Json{},
                             120, 30, 240);
            const double framesPerStep = sampleRate * 60.0 / (bpm * 4.0);
            int lastStep = song.contains("bars") && song["bars"].is_number()
                               ? std::max(song["bars"].as_int() * 16, 16)
                               : 128;

            // Master chain rebuild: absent key -> empty chain (old path).
            {
                const auto slots = SongDocument::effect_chain(song);
                std::vector<std::pair<std::string, bool>> types;
                std::vector<std::vector<std::pair<int, int>>> params;
                for (const auto &s : slots)
                {
                    types.emplace_back(s.type, s.bypass);
                    params.push_back(s.params);
                }
                if (!types.empty())
                    buildChainFromTypes(double(sampleRate), types, params, masterChain);
                else
                    masterChain = EffectChain{};
                // The scratch must be sized here or the realtime chain never runs:
                // render() only processes when the scratch covers the callback.
                chainScratchL.assign(static_cast<size_t>(sampleRate), 0.f);
                chainScratchR.assign(static_cast<size_t>(sampleRate), 0.f);
            }

            if (!coreVoice)
                coreVoice = std::make_unique<SynthCoreVoice>();
            coreVoice->setup(coreKindForSong(song), static_cast<double>(sampleRate));

            std::vector<CoreNoteEvent> events;
            int trackIndex = 0;
            for (const Json &track : song["tracks"].as_array())
            {
                if (!track.contains("notes") || !track["notes"].is_array())
                    continue;
                const int trackVolume = clampedInt(
                    track.contains("volume") ? Json(track["volume"]) : Json{}, 90, 0, 127);
                const int panValue = clampedInt(
                    track.contains("pan") ? Json(track["pan"]) : Json{}, 64, 0, 127);
                const int program = clampedInt(
                    track.contains("program") ? Json(track["program"]) : Json{}, 0, 0, 127);
                for (const Json &note : track["notes"].as_array())
                {
                    if (!note.is_array() || note.as_array().size() < 3)
                        continue;
                    const int step = std::max(note[0].as_int(), 0);
                    const int length = std::clamp(note[2].as_int(), 1, 512);
                    const int velocity = note.as_array().size() >= 4
                                             ? std::clamp(note[3].as_int(), 0, 127)
                                             : 90;
                    CoreNoteEvent event;
                    event.startFrame = baseFrame +
                        static_cast<std::int64_t>(step * framesPerStep);
                    event.endFrame = event.startFrame +
                        static_cast<std::int64_t>(length * framesPerStep);
                    event.track = trackIndex;
                    event.pitch = std::clamp(note[1].as_int(), 0, 127);
                    event.velocity = std::max(1, velocity * trackVolume / 127);
                    event.pan = panValue;
                    event.program = program;
                    events.push_back(event);
                    lastStep = std::max(lastStep, step + length);
                }
                ++trackIndex;
            }
            // The step patterns ride the same event list, so the groovebox and
            // the piano roll play through the same instrument.
            for (const StepEvent &pattern : compile_chain(song))
            {
                CoreNoteEvent event;
                event.startFrame = baseFrame +
                    static_cast<std::int64_t>(std::max(0.0, pattern.step) * framesPerStep);
                event.endFrame = event.startFrame +
                    static_cast<std::int64_t>(std::max(0.1, pattern.length) * framesPerStep);
                event.track = pattern.track;
                event.pitch = std::clamp(pattern.pitch, 0, 127);
                event.velocity = std::clamp(pattern.velocity, 1, 127);
                event.pan = 64;
                event.program = std::clamp(pattern.program, 0, 127);
                events.push_back(event);
                lastStep = std::max(lastStep, static_cast<int>(std::ceil(pattern.step + pattern.length)));
            }
            coreVoice->setEvents(std::move(events));

            loopSteps = lastStep;
            nextWrapFrame = baseFrame +
                static_cast<std::int64_t>(lastStep * framesPerStep);
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
                std::vector<CoreNoteEvent> auditions;
                for (const CoreNoteEvent &a : pendingAuditions)
                {
                    CoreNoteEvent v = a;
                    const std::int64_t length =
                        static_cast<std::int64_t>(v.endFrame * auditionFramesPerStep);
                    v.startFrame = frameCounter + frames;
                    v.endFrame = v.startFrame + length;
                    auditions.push_back(v);
                }
                pendingAuditions.clear();
                // The auditions join the core's own event list, so they survive
                // the next loop wrap like every other note.
                coreVoice->appendEvents(std::move(auditions));
            }

            double peakLeft = 0.0;
            double peakRight = 0.0;
            for (int i = 0; i < frames; ++i, ++frameCounter)
            {
                if (loopSteps > 0 && frameCounter >= nextWrapFrame)
                {
                    // Replay the looped song: rebase all events to the new pass
                    // and reset the core's voice/envelope state.
                    coreVoice->reset();
                    coreVoice->rebaseEvents(nextWrapFrame - static_cast<std::int64_t>(frameCounter));
                    nextWrapFrame += static_cast<std::int64_t>(loopSteps * sampleRate * 60.0 / (bpm * 4.0));
                }
                coreVoice->consumeUpTo(frameCounter);
                float cl = 0.f, cr = 0.f;
                coreVoice->renderFrame(cl, cr);
                // The master volume is applied after the clip, so a quiet mix
                // stays quiet instead of clipping.
                const float gain = masterGain.load(std::memory_order_relaxed);
                const float l = static_cast<float>(std::clamp(cl, -0.95f, 0.95f)) * gain;
                const float r = static_cast<float>(std::clamp(cr, -0.95f, 0.95f)) * gain;
                out[2 * i] = l;
                out[2 * i + 1] = r;
                peakLeft = std::max(peakLeft, std::abs(static_cast<double>(l)));
                peakRight = std::max(peakRight, std::abs(static_cast<double>(r)));
            }
            if (!masterChain.empty() && chainScratchL.size() >= size_t(frames))
            {
                float pl = 0.f, pr = 0.f;
                masterChain.processInterleaved(out, size_t(frames),
                                               chainScratchL.data(), chainScratchR.data(),
                                               pl, pr);
                peakLeft = std::max(peakLeft, double(pl));
                peakRight = std::max(peakRight, double(pr));
                chainLevelLeft.store(pl, std::memory_order_relaxed);
                chainLevelRight.store(pr, std::memory_order_relaxed);
                chainBudget.store(masterChain.budget(), std::memory_order_relaxed);
                masterChain.resetBudget();
            }
            else
            {
                chainLevelLeft.store(peakLeft, std::memory_order_relaxed);
                chainLevelRight.store(peakRight, std::memory_order_relaxed);
                chainBudget.store(0, std::memory_order_relaxed);
            }
            levelLeft.store(peakLeft, std::memory_order_relaxed);
            levelRight.store(peakRight, std::memory_order_relaxed);
            playheadFrames.store(frameCounter, std::memory_order_relaxed);
            playheadBpm.store(bpm, std::memory_order_relaxed);
            playheadActive.store(true, std::memory_order_relaxed);
        }

    } // namespace audio
} // namespace aur
