#include "aur/audio/synth.hpp"
#include "aur/audio/EffectChain.hpp"
#include "aur/audio/SongDocument.hpp"
#include "aur/audio/SynthCores.hpp"
#include "aur/audio/VoiceCore.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aur
{
    namespace
    {

        constexpr double pi = 3.14159265358979323846;


        // Renders a song through one of the hardware cores (MdFm or Spc).
        // Notes are translated to core MIDI events at frame boundaries; the
        // core renders every frame so its envelopes/LFO run continuously.
        StereoPcm synthesize_core_preview(const Json &song, PreviewSynth synth,
                                          int sample_rate, int total_frames,
                                          int bpm, const std::atomic<bool> *cancel,
                                          const RenderProgress &progress)
        {
            aur::audio::SynthCoreVoice core;
            core.setup(synth == PreviewSynth::MdFm ? aur::audio::SynthCoreVoice::Kind::MdFm
                                                   : aur::audio::SynthCoreVoice::Kind::Spc,
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
            if (events.empty())
                throw std::runtime_error("Song has no notes to synthesize.");
            core.setEvents(std::move(events));

            StereoPcm output;
            output.sample_rate = sample_rate;
            output.samples.reserve(static_cast<size_t>(total_frames) * 2);
            for (int frame = 0; frame < total_frames; ++frame)
            {
                if (cancel && cancel->load(std::memory_order_relaxed))
                    return StereoPcm{};
                if (progress && (frame & 0xfff) == 0)
                    progress(static_cast<double>(frame) / total_frames);
                core.consumeUpTo(frame);
                float left = 0.f, right = 0.f;
                core.renderFrame(left, right);
                output.samples.push_back(static_cast<int16_t>(
                    std::clamp(left, -0.95f, 0.95f) * 32767.0f));
                output.samples.push_back(static_cast<int16_t>(
                    std::clamp(right, -0.95f, 0.95f) * 32767.0f));
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
        std::vector<audio::Voice> voices;
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
            const bool drum = (track.contains("channel") && track["channel"].is_number() &&
                               track["channel"].as_int() == 9) ||
                              (track.contains("drum") && track["drum"].is_bool() &&
                               track["drum"].as_bool());
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
                const int start = static_cast<int>(
                    step * sample_rate * 60.0 / (bpm * 4.0));
                const int end = start + static_cast<int>(
                                            length * sample_rate * 60.0 / (bpm * 4.0));
                const int pan_value = track.contains("pan") && track["pan"].is_number()
                                          ? std::clamp(track["pan"].as_int(), 0, 127)
                                          : 64;
                audio::Voice voice;
                voice.start = start;
                voice.end = end;
                voice.frequency = 440.0 * std::pow(2.0, (pitch - 69) / 12.0);
                voice.amplitude = 0.14 * track_volume / 127.0 * velocity / 127.0;
                voice.pan = pan_value / 127.0;
                voice.program = program;
                voice.pitch = pitch;
                voice.drum = drum;
                voices.push_back(voice);
                last_step = std::max(last_step, step + length);
            }
        }
        if (voices.empty())
            throw std::runtime_error("Song has no notes to synthesize.");

        const int total_frames = std::min(sample_rate * max_seconds,
                                          static_cast<int>((last_step + 4) * sample_rate * 60.0 / (bpm * 4.0)));
        if (synth == PreviewSynth::MdFm || synth == PreviewSynth::Spc)
        {
            return synthesize_core_preview(song, synth, sample_rate, total_frames,
                                           bpm, cancel, progress);
        }
        std::vector<std::vector<size_t>> starts(static_cast<size_t>(total_frames + 1));
        for (size_t index = 0; index < voices.size(); ++index)
        {
            if (voices[index].start < total_frames)
            {
                starts[static_cast<size_t>(voices[index].start)].push_back(index);
            }
        }
        StereoPcm output;
        output.sample_rate = sample_rate;
        output.samples.reserve(static_cast<size_t>(total_frames) * 2);
        // Master chain runs on float frames before quantisation so the offline
        // path matches the realtime mixer (which is float throughout). No chain
        // -> the loop below quantises exactly as before.
        audio::EffectChain masterChain;
        {
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
            }
        }
        std::vector<float> chainL, chainR;
        if (!masterChain.empty())
        {
            chainL.resize(static_cast<size_t>(total_frames));
            chainR.resize(static_cast<size_t>(total_frames));
        }
        std::vector<size_t> active;
        for (int frame = 0; frame < total_frames; ++frame)
        {
            if (cancel && cancel->load(std::memory_order_relaxed))
                return StereoPcm{};
            if (progress && (frame & 0xfff) == 0)
                progress(static_cast<double>(frame) / total_frames);
            active.insert(active.end(), starts[static_cast<size_t>(frame)].begin(),
                          starts[static_cast<size_t>(frame)].end());
            double left = 0.0;
            double right = 0.0;
            for (auto it = active.begin(); it != active.end();)
            {
                audio::Voice &voice = voices[*it];
                if (frame >= voice.end)
                {
                    it = active.erase(it);
                    continue;
                }
                const double time = static_cast<double>(frame - voice.start) / sample_rate;
                const double value = audio::render_voice_sample(
                    voice, static_cast<audio::Timbre>(synth), time, frame, sample_rate);
                const double gain = voice.amplitude *
                                    audio::voice_envelope(voice, frame, sample_rate);
                left += value * gain * std::cos(voice.pan * pi / 2.0);
                right += value * gain * std::sin(voice.pan * pi / 2.0);
                ++it;
            }
            if (!masterChain.empty())
            {
                // Realtime feeds the chain from the same clamped ±0.95 samples
                // the mixer writes out, so keep the offline side identical.
                chainL[static_cast<size_t>(frame)] = static_cast<float>(
                    std::clamp(left, -0.95, 0.95));
                chainR[static_cast<size_t>(frame)] = static_cast<float>(
                    std::clamp(right, -0.95, 0.95));
            }
            else
            {
                output.samples.push_back(static_cast<int16_t>(
                    std::clamp(left, -0.95, 0.95) * 32767.0));
                output.samples.push_back(static_cast<int16_t>(
                    std::clamp(right, -0.95, 0.95) * 32767.0));
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

} // namespace aur
