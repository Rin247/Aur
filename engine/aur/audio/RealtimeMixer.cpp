#include "aur/audio/RealtimeMixer.hpp"
#include "aur/audio/SynthCores.hpp"
#include "aur/audio/SongDocument.hpp"
#include "aur/audio/StepSequencer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

            bool isCoreSynth(const Json &song)
            {
                const std::string synthId = song.contains("preview_synth") &&
                                                    song["preview_synth"].is_string()
                                                ? song["preview_synth"].as_string()
                                                : "gm";
                return synthId == "md_fm" || synthId == "spc";
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
            coreSynth = isCoreSynth(song);
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
                // The scratch must be sized here or the realtime chain never
                // runs: render() only processes when the scratch covers the
                // whole callback block.
                chainScratchL.assign(static_cast<size_t>(sampleRate), 0.f);
                chainScratchR.assign(static_cast<size_t>(sampleRate), 0.f);
            }
            if (coreSynth)
            {
                // Whole-song core rendering: translate the schedule once and
                // let the core consume note events as frames advance.
                if (!coreVoice)
                    coreVoice = std::make_unique<SynthCoreVoice>();
                const SynthCoreVoice::Kind kind =
                    (song.contains("preview_synth") && song["preview_synth"].is_string() &&
                     song["preview_synth"].as_string() == "spc")
                        ? SynthCoreVoice::Kind::Spc
                        : SynthCoreVoice::Kind::MdFm;
                coreVoice->setup(kind, static_cast<double>(sampleRate));
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
                coreVoice->setEvents(std::move(events));
                loopSteps = lastStep;
                nextWrapFrame = baseFrame +
                    static_cast<std::int64_t>(lastStep * framesPerStep);
                loopVoices.clear();
                return;
            }
            if (!song.contains("tracks") || !song["tracks"].is_array())
            {
                schedulePatternVoices(song, baseFrame);
                return;
            }
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
            schedulePatternVoices(song, baseFrame);
            lastStep = std::max(lastStep, patternLastStep);
            loopSteps = lastStep;
            nextWrapFrame = baseFrame +
                static_cast<std::int64_t>(lastStep * framesPerStep);
            loopVoices = std::vector<Voice>(upcoming.begin(), upcoming.end());
            // The activation loop below stops at the first voice whose start
            // is in the future, so the queue must be sorted by start frame.
            std::stable_sort(loopVoices.begin(), loopVoices.end(),
                             [](const Voice &a, const Voice &b)
                             { return a.start < b.start; });
            upcoming.assign(loopVoices.begin(), loopVoices.end());
        }

        void RealtimeMixer::schedulePatternVoices(const Json &song, std::int64_t baseFrame)
        {
            patternLastStep = 0;
            const auto events = compile_chain(song);
            if (events.empty())
                return;
            const double framesPerStep = sampleRate * 60.0 / (bpm * 4.0);
            for (const StepEvent &event : events)
            {
                const double startStep = std::max(0.0, event.step);
                Voice voice;
                voice.start = baseFrame + static_cast<std::int64_t>(startStep * framesPerStep);
                voice.end = voice.start +
                    static_cast<std::int64_t>(std::max(0.1, event.length) * framesPerStep);
                voice.pitch = std::clamp(event.pitch, 0, 127);
                voice.frequency = 440.0 * std::pow(2.0, (voice.pitch - 69) / 12.0);
                voice.amplitude = 0.14 * event.velocity / 127.0;
                voice.pan = 0.5;
                voice.program = std::clamp(event.program, 0, 127);
                voice.drum = event.channel == 9;
                upcoming.push_back(voice);
                patternLastStep = std::max(patternLastStep,
                                           static_cast<int>(std::ceil(startStep + event.length)));
            }
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
                // Auditions land at the head of the queue; re-sort so the
                // activation loop below never stops early.
                std::stable_sort(upcoming.begin(), upcoming.end(),
                                 [](const Voice &a, const Voice &b)
                                 { return a.start < b.start; });
            }

            const double framesPerStep = sampleRate * 60.0 / (bpm * 4.0);
            const std::int64_t loopFrames =
                loopSteps > 0 ? static_cast<std::int64_t>(loopSteps * framesPerStep) : 0;
            double peakLeft = 0.0;
            double peakRight = 0.0;

            if (coreSynth && coreVoice)
            {
                // Core path: one instance renders every frame; note events are
                // consumed at frame boundaries. Loop wrap replays the song by
                // resetting the core and its schedule at the wrap frame.
                for (int i = 0; i < frames; ++i, ++frameCounter)
                {
                    if (loopFrames > 0 && frameCounter >= nextWrapFrame)
                    {
                        // Replay the looped song: rebase all events to the new
                        // pass and reset the core's voice/envelope state.
                        coreVoice->reset();
                        coreVoice->rebaseEvents(nextWrapFrame - static_cast<std::int64_t>(frameCounter));
                        nextWrapFrame += loopFrames;
                    }
                    coreVoice->consumeUpTo(frameCounter);
                    float cl = 0.f, cr = 0.f;
                    coreVoice->renderFrame(cl, cr);
                    const float l = static_cast<float>(std::clamp(cl, -0.95f, 0.95f));
                    const float r = static_cast<float>(std::clamp(cr, -0.95f, 0.95f));
                    out[2 * i] = l;
                    out[2 * i + 1] = r;
                    peakLeft = std::max(peakLeft, std::abs(static_cast<double>(l)));
                    peakRight = std::max(peakRight, std::abs(static_cast<double>(r)));
                }
                if (!masterChain.empty())
                {
                    float pl = 0.f, pr = 0.f;
                    if (chainScratchL.size() >= size_t(frames))
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
                return;
            }

            // Activate voices whose start has arrived (upcoming kept sorted at build).
            for (auto it = upcoming.begin(); it != upcoming.end() &&
                                             it->start <= frameCounter + frames;)
            {
                voices.push_back(*it);
                it = upcoming.erase(it);
            }

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
                    // The wrapped voices are appended, so the queue must be
                    // re-sorted before the activation scan below.
                    std::stable_sort(upcoming.begin(), upcoming.end(),
                                     [](const Voice &a, const Voice &b)
                                     { return a.start < b.start; });
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
