#include "aur/audio/synth.hpp"

#include "aur/audio/EffectChain.hpp"

#include "aur/audio/SongDocument.hpp"

#include "aur/audio/StepSequencer.hpp"

#include "aur/audio/SynthCores.hpp"

#include <algorithm>
#include <cmath>

#include <stdexcept>

namespace aur
{
    namespace
    {

        // Renders a song through one of the hardware cores (MdFm or Spc).
        // Notes become core MIDI events at frame boundaries; the core renders every frame.
        StereoPcm synthesize_core_preview(const Json &song, PreviewSynth synth,
                                           int sample_rate, int total_frames,
                                           int bpm, const std::atomic<bool> *cancel,
                                           const RenderProgress &progress)
        {
            aur::audio::SynthCoreVoice core;
            core.setup(synth == PreviewSynth::Spc ? aur::audio::SynthCoreVoice::Kind::Spc
                                                   : aur::audio::SynthCoreVoice::Kind::MdFm,
                       sample_rate);

            const double framesPerStep = sample_rate * 60.0 / (bpm * 4.0);
            std::vector<aur::audio::CoreNoteEvent> events;
            int trackIndex = 0;
            for (const Json &track : song["tracks"].as_array())
            {
                if (!track.contains("notes") || !track["notes"].is_array())
                    continue;
                const int track_volume = track.contains("volume") && track["volume"].is_number()
                                             ? std::clamp(track["volume"].as_int(), 0, 127)
                                             : 90;
                const int program = track.contains("program") && track["program"].is_number()
                                        ? std::clamp(track["program"].as_int(), 0, 127)
                                        : 0;
                const int pan_value = track.contains("pan") && track["pan"].is_number()
                                          ? std::clamp(track["pan"].as_int(), 0, 127)
                                          : 64;
                for (const Json &note : track["notes"].as_array())
                {
                    if (!note.is_array() || note.as_array().size() < 3)
                        continue;
                    const int step = std::max(note[0].as_int(), 0);
                    const int pitch = std::clamp(note[1].as_int(), 0, 127);
                    const int length = std::clamp(note[2].as_int(), 1, 512);
                    const int velocity = note.as_array().size() >= 4
                                             ? std::clamp(note[3].as_int(), 0, 127)
                                             : 90;
                    aur::audio::CoreNoteEvent event;
                    event.startFrame = static_cast<std::int64_t>(step * framesPerStep);
                    event.endFrame = event.startFrame +
                                     static_cast<std::int64_t>(length * framesPerStep);
                    event.track = trackIndex;
                    event.pitch = pitch;
                    event.velocity = std::max(1, velocity * track_volume / 127);
                    event.pan = pan_value;
                    event.program = program;
                    events.push_back(event);
                }
                ++trackIndex;
            }
            // Step patterns ride the same core path, so a pattern-bearing
            // song exports through the hardware core too.
            for (const aur::audio::StepEvent &pattern : aur::audio::compile_chain(song))
            {
                aur::audio::CoreNoteEvent event;
                event.startFrame = static_cast<std::int64_t>(
                    std::max(0.0, pattern.step) * framesPerStep);
                event.endFrame = event.startFrame + static_cast<std::int64_t>(
                    std::max(0.1, pattern.length) * framesPerStep);
                event.track = pattern.track;
                event.pitch = std::clamp(pattern.pitch, 0, 127);
                event.velocity = std::clamp(pattern.velocity, 1, 127);
                event.pan = 64;
                event.program = std::clamp(pattern.program, 0, 127);
                events.push_back(event);
            }
            if (events.empty())
                throw std::runtime_error("Song has no notes to synthesize.");
            std::stable_sort(events.begin(), events.end(),
                             [](const aur::audio::CoreNoteEvent &a,
                                const aur::audio::CoreNoteEvent &b)
                             { return a.startFrame < b.startFrame; });
            core.setEvents(std::move(events));

            StereoPcm output;
            output.sample_rate = sample_rate;
            output.samples.reserve(static_cast<size_t>(total_frames) * 2);
            std::vector<float> chainL, chainR;
            audio::EffectChain masterChain;
            const auto slots = audio::SongDocument::effect_chain(song);
            if (!slots.empty())
            {
                std::vector<std::pair<std::string, bool>> types;
                std::vector<std::vector<std::pair<int, int>>> params;
                for (const auto &s : slots)
                {
                    types.emplace_back(s.type, s.bypass);
                    params.push_back(s.params);
                }
                audio::buildChainFromTypes(double(sample_rate), types, params, masterChain);
                chainL.resize(static_cast<size_t>(total_frames));
                chainR.resize(static_cast<size_t>(total_frames));
            }
            for (int frame = 0; frame < total_frames; ++frame)
            {
                if (cancel && cancel->load(std::memory_order_relaxed))
                    return StereoPcm{};
                if (progress && (frame & 0xfff) == 0)
                    progress(static_cast<double>(frame) / total_frames);
                core.consumeUpTo(frame);
                float left = 0.f, right = 0.f;
                core.renderFrame(left, right);
                if (!masterChain.empty())
                {
                    chainL[static_cast<size_t>(frame)] = std::clamp(left, -0.95f, 0.95f);
                    chainR[static_cast<size_t>(frame)] = std::clamp(right, -0.95f, 0.95f);
                }
                else
                {
                    output.samples.push_back(static_cast<int16_t>(
                        std::clamp(left, -0.95f, 0.95f) * 32767.0f));
                    output.samples.push_back(static_cast<int16_t>(
                        std::clamp(right, -0.95f, 0.95f) * 32767.0f));
                }
            }
            if (!masterChain.empty())
            {
                float pl = 0.f, pr = 0.f;
                masterChain.process(chainL.data(), chainR.data(), chainL.size(), pl, pr);
                for (size_t i = 0; i < chainL.size(); ++i)
                {
                    output.samples.push_back(static_cast<int16_t>(
                        std::clamp(chainL[i], -0.95f, 0.95f) * 32767.0f));
                    output.samples.push_back(static_cast<int16_t>(
                        std::clamp(chainR[i], -0.95f, 0.95f) * 32767.0f));
                }
            }
            if (progress)
                progress(1.0);
            return output;
        }

    } // namespace

    StereoPcm synthesize_preview(const Json &song, PreviewSynth synth,
                                 int sample_rate, int max_seconds,
                                 const std::atomic<bool> *cancel,
                                 const RenderProgress &progress)
    {
        if (!song.contains("tracks") || !song["tracks"].is_array())
        {
            throw std::runtime_error("Song has no tracks to synthesize.");
        }
        if (sample_rate < 8000 || sample_rate > 192000 || max_seconds < 1 || max_seconds > 600)
        {
            throw std::runtime_error("Synth sample rate or duration is outside supported limits.");
        }

        const int bpm = song.contains("tempo") && song["tempo"].is_number()
                            ? std::clamp(song["tempo"].as_int(), 30, 240)
                            : 120;
        int last_step = song.contains("bars") && song["bars"].is_number()
                            ? std::max(song["bars"].as_int() * 16, 16)
                            : 128;
        for (const Json &track : song["tracks"].as_array())
        {
            if (!track.contains("notes") || !track["notes"].is_array())
                continue;
            for (const Json &note : track["notes"].as_array())
            {
                if (!note.is_array() || note.as_array().size() < 3)
                    continue;
                const int step = std::max(note[0].as_int(), 0);
                const int length = std::clamp(note[2].as_int(), 1, 512);
                last_step = std::max(last_step, step + length);
            }
        }
        for (const auto &event : audio::compile_chain(song))
            last_step = std::max(last_step, static_cast<int>(std::ceil(event.step + event.length)));

        const int total_frames = std::min(sample_rate * max_seconds,
                                          static_cast<int>((last_step + 4) * sample_rate * 60.0 / (bpm * 4.0)));
        return synthesize_core_preview(song, synth, sample_rate, total_frames,
                                       bpm, cancel, progress);
    }

} // namespace aur
