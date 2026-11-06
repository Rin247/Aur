#include "synth.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aur {
namespace {

constexpr double pi = 3.14159265358979323846;

struct NoteVoice {
    int start = 0;
    int end = 0;
    double frequency = 0.0;
    double amplitude = 0.0;
    double pan = 0.5;
    double phase[6]{};
    int program = 0;
    int pitch = 0;
    bool drum = false;
};

double envelope(const NoteVoice& voice, int frame, int sample_rate) {
    const int attack = std::max(1, sample_rate / 200);
    const int release = std::max(1, sample_rate / 8);
    const double attack_gain = std::clamp(
        static_cast<double>(frame - voice.start) / attack, 0.0, 1.0);
    const double release_gain = std::clamp(
        static_cast<double>(voice.end - frame) / release, 0.0, 1.0);
    return attack_gain * release_gain;
}

double fm_sample(NoteVoice& voice, double time, int sample_rate) {
    const int algorithm = voice.program % 4;
    double operators[6]{};
    for (int op = 0; op < 6; ++op) {
        const double ratio = 0.5 + ((voice.program * (op + 3) + op * 7) % 16) * 0.25;
        const double decay = std::exp(-time * (0.6 + ((voice.program + op * 5) % 8) * 0.28));
        const double level = 0.32 + ((voice.program + op * 11) % 9) * 0.11;
        const double phase_increment = 2.0 * pi * voice.frequency * ratio / sample_rate;
        voice.phase[op] = std::fmod(voice.phase[op] + phase_increment, 2.0 * pi);
        const double feedback = (0.35 + (voice.program % 5) * 0.12) * decay;
        operators[op] = std::sin(voice.phase[op] + feedback *
            (op == 0 ? 0.0 : operators[op - 1])) * level * decay;
    }

    switch (algorithm) {
    case 0:
        return std::sin(voice.phase[5] + operators[4] * 4.0);
    case 1:
        return (std::sin(voice.phase[5] + operators[4] * 3.0) +
                std::sin(voice.phase[3] + operators[2] * 3.0) +
                std::sin(voice.phase[1] + operators[0] * 2.0)) / 3.0;
    case 2:
        return (std::sin(voice.phase[5] + operators[4] * 2.2 +
                         operators[2] * 1.8 + operators[0] * 1.3) +
                std::sin(voice.phase[3] + operators[1] * 1.7)) * 0.5;
    default:
        return (std::sin(voice.phase[0]) + std::sin(voice.phase[1]) +
                std::sin(voice.phase[2]) + std::sin(voice.phase[3]) +
                std::sin(voice.phase[4]) + std::sin(voice.phase[5])) / 6.0;
    }
}

double psg_sample(NoteVoice& voice, int sample_rate) {
    const double phase = voice.phase[0] / (2.0 * pi);
    double sample = 0.0;
    switch (voice.program % 4) {
    case 0:
        sample = phase < 0.5 ? 1.0 : -1.0;
        break;
    case 1:
        sample = phase < 0.25 ? 1.0 : -1.0;
        break;
    case 2:
        sample = 1.0 - 4.0 * std::abs(phase - 0.5);
        break;
    default:
        sample = 2.0 * phase - 1.0;
        break;
    }
    voice.phase[0] = std::fmod(
        voice.phase[0] + 2.0 * pi * voice.frequency / sample_rate, 2.0 * pi);
    return sample * 0.72;
}

double gm_sample(NoteVoice& voice, double time, int frame, int sample_rate) {
    const int family = voice.program / 8;
    const double phase = voice.phase[0] / (2.0 * pi);
    const double brightness = 0.42 + (voice.program % 8) * 0.065;
    double sample = 0.0;
    if (voice.drum) {
        uint32_t noise_state = static_cast<uint32_t>(frame) ^
            static_cast<uint32_t>(voice.program * 0x9e3779b9u) ^
            static_cast<uint32_t>(std::lround(voice.frequency * 31.0));
        noise_state ^= noise_state << 13;
        noise_state ^= noise_state >> 17;
        noise_state ^= noise_state << 5;
        const double noise = static_cast<double>(noise_state & 0xffffu) / 32767.5 - 1.0;
        if (voice.pitch <= 36) {
            const double kick_frequency = 52.0 + 74.0 * std::exp(-time * 18.0);
            sample = std::sin(voice.phase[0]) * std::exp(-time * 5.2) +
                     noise * std::exp(-time * 35.0) * 0.22;
            voice.phase[0] = std::fmod(
                voice.phase[0] + 2.0 * pi * kick_frequency / sample_rate, 2.0 * pi);
        } else if (voice.pitch == 42 || voice.pitch == 44 || voice.pitch == 46) {
            sample = noise * std::exp(-time * (voice.pitch == 46 ? 5.0 : 13.0));
        } else {
            sample = (noise * 0.68 + std::sin(voice.phase[0]) * 0.32) *
                     std::exp(-time * (voice.pitch <= 40 ? 7.0 : 5.0));
            voice.phase[0] = std::fmod(
                voice.phase[0] + 2.0 * pi * voice.frequency / sample_rate, 2.0 * pi);
        }
        return sample;
    }

    voice.phase[0] = std::fmod(
        voice.phase[0] + 2.0 * pi * voice.frequency / sample_rate, 2.0 * pi);
    switch (family) {
    case 0:
        sample = std::sin(voice.phase[0]) * 0.76 +
                 std::sin(voice.phase[0] * 2.003) * brightness * 0.22 +
                 std::sin(voice.phase[0] * 3.01) * brightness * 0.09;
        break;
    case 1:
        sample = std::sin(voice.phase[0]) * 0.58 +
                 std::sin(voice.phase[0] * 2.71) * 0.32 +
                 std::sin(voice.phase[0] * 4.13) * 0.16;
        break;
    case 2:
        for (int harmonic = 1; harmonic <= 6; ++harmonic) {
            const double drawbar = ((voice.program + harmonic * 3) % 7) < 4 ? 1.0 : 0.42;
            sample += std::sin(voice.phase[0] * harmonic) * drawbar / (harmonic * 1.7);
        }
        break;
    case 3:
    case 4:
    case 13:
        sample = std::sin(voice.phase[0]) * 0.68 +
                 std::sin(voice.phase[0] * 2.0) * brightness * 0.25 +
                 std::sin(voice.phase[0] * 3.0) * brightness * 0.13;
        break;
    case 5:
    case 6:
        sample = (2.0 * phase - 1.0) * 0.24 +
                 std::sin(voice.phase[0] * 2.0) * 0.20 +
                 std::sin(voice.phase[0] * 3.0) * 0.12 +
                 std::sin(voice.phase[0] + std::sin(time * 4.1) * 0.025) * 0.48;
        break;
    case 7:
        sample = (2.0 * phase - 1.0) * 0.38 +
                 std::sin(voice.phase[0] * 2.0) * 0.25 +
                 std::sin(voice.phase[0] * 3.0) * 0.18 +
                 std::sin(voice.phase[0] * 4.0) * 0.08;
        break;
    case 8:
        sample = std::sin(voice.phase[0]) * 0.72 +
                 std::sin(voice.phase[0] * 2.0) * 0.16 +
                 std::sin(voice.phase[0] * 3.0) * 0.08;
        break;
    case 9:
        sample = std::sin(voice.phase[0]) * 0.82 +
                 std::sin(voice.phase[0] * 2.0) * 0.12 +
                 std::sin(voice.phase[0] * 4.0) * 0.06;
        break;
    case 10:
        sample = (voice.program % 2 == 0 ? 2.0 * phase - 1.0
                                          : (phase < 0.5 ? 1.0 : -1.0)) * 0.46 +
                 std::sin(voice.phase[0]) * 0.40 +
                 std::sin(voice.phase[0] * 3.0) * brightness * 0.16;
        break;
    case 11:
        sample = std::sin(voice.phase[0]) * 0.65 +
                 std::sin(voice.phase[0] * 1.006) * 0.22 +
                 std::sin(voice.phase[0] * 2.0) * 0.18;
        break;
    case 12:
    case 15:
        sample = std::sin(voice.phase[0]) * 0.58 +
                 std::sin(voice.phase[0] * (2.0 + (voice.program % 5) * 0.31)) * 0.24 +
                 std::sin(voice.phase[0] * 5.0) * 0.12;
        break;
    case 14:
        sample = (phase < 0.5 ? 1.0 : -1.0) * 0.42 +
                 std::sin(voice.phase[0] * 3.0) * 0.38 +
                 std::sin(voice.phase[0] * 5.0) * 0.18;
        break;
    }

    double attack = 0.008;
    double decay = 0.0;
    if (family == 5 || family == 6 || family == 11) attack = 0.12;
    else if (family == 7 || family == 8) attack = 0.035;
    else if (family == 2 || family == 9) attack = 0.018;
    else if (family == 10 || family == 12) attack = 0.025;
    if (family == 0) decay = 1.1 + (voice.program % 4) * 0.35;
    else if (family == 3 || family == 4 || family == 13) decay = 2.2;
    else if (family == 1 || family == 14) decay = 3.8;
    const double attack_gain = std::clamp(time / attack, 0.0, 1.0);
    const double decay_gain = decay > 0.0
        ? 0.42 + 0.58 * std::exp(-time * decay) : 1.0;
    return sample * attack_gain * decay_gain;
}

} // namespace

StereoPcm synthesize_preview(const Json& song, PreviewSynth synth,
                             int sample_rate, int max_seconds) {
    if (!song.contains("tracks") || !song["tracks"].is_array()) {
        throw std::runtime_error("Song has no tracks to synthesize.");
    }
    if (sample_rate < 8000 || sample_rate > 192000 || max_seconds < 1 || max_seconds > 600) {
        throw std::runtime_error("Synth sample rate or duration is outside supported limits.");
    }

    const int bpm = song.contains("tempo") && song["tempo"].is_number()
        ? std::clamp(song["tempo"].as_int(), 30, 240) : 120;
    int last_step = song.contains("bars") && song["bars"].is_number()
        ? std::max(song["bars"].as_int() * 16, 16)
        : 128;
    std::vector<NoteVoice> voices;
    for (const Json& track : song["tracks"].as_array()) {
        if (!track.contains("notes") || !track["notes"].is_array()) continue;
        const int track_volume = track.contains("volume") && track["volume"].is_number()
            ? std::clamp(track["volume"].as_int(), 0, 127) : 90;
        const int program = track.contains("program") && track["program"].is_number()
            ? std::clamp(track["program"].as_int(), 0, 127) : 0;
        const bool drum = (track.contains("channel") && track["channel"].is_number() &&
                           track["channel"].as_int() == 9) ||
                          (track.contains("drum") && track["drum"].is_bool() &&
                           track["drum"].as_bool());
        for (const Json& note : track["notes"].as_array()) {
            if (!note.is_array() || note.as_array().size() < 3) continue;
            const int step = std::max(note[0].as_int(), 0);
            const int pitch = std::clamp(note[1].as_int(), 0, 127);
            const int length = std::clamp(note[2].as_int(), 1, 512);
            const int velocity = note.as_array().size() >= 4
                ? std::clamp(note[3].as_int(), 0, 127) : 90;
            const int start = static_cast<int>(
                step * sample_rate * 60.0 / (bpm * 4.0));
            const int end = start + static_cast<int>(
                length * sample_rate * 60.0 / (bpm * 4.0));
            const int pan_value = track.contains("pan") && track["pan"].is_number()
                ? std::clamp(track["pan"].as_int(), 0, 127) : 64;
            NoteVoice voice;
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
    if (voices.empty()) throw std::runtime_error("Song has no notes to synthesize.");

    const int total_frames = std::min(sample_rate * max_seconds,
        static_cast<int>((last_step + 4) * sample_rate * 60.0 / (bpm * 4.0)));
    std::vector<std::vector<size_t>> starts(static_cast<size_t>(total_frames + 1));
    for (size_t index = 0; index < voices.size(); ++index) {
        if (voices[index].start < total_frames) {
            starts[static_cast<size_t>(voices[index].start)].push_back(index);
        }
    }
    StereoPcm output;
    output.sample_rate = sample_rate;
    output.samples.reserve(static_cast<size_t>(total_frames) * 2);
    std::vector<size_t> active;
    for (int frame = 0; frame < total_frames; ++frame) {
        active.insert(active.end(), starts[static_cast<size_t>(frame)].begin(),
                      starts[static_cast<size_t>(frame)].end());
        double left = 0.0;
        double right = 0.0;
        for (auto it = active.begin(); it != active.end();) {
            NoteVoice& voice = voices[*it];
            if (frame >= voice.end) {
                it = active.erase(it);
                continue;
            }
            const double time = static_cast<double>(frame - voice.start) / sample_rate;
            double value = 0.0;
            if (synth == PreviewSynth::Fm) {
                value = fm_sample(voice, time, sample_rate);
            } else if (synth == PreviewSynth::RetroPsg) {
                value = psg_sample(voice, sample_rate);
            } else {
                value = gm_sample(voice, time, frame, sample_rate);
            }
            const double gain = voice.amplitude * envelope(voice, frame, sample_rate);
            left += value * gain * std::cos(voice.pan * pi / 2.0);
            right += value * gain * std::sin(voice.pan * pi / 2.0);
            ++it;
        }
        output.samples.push_back(static_cast<int16_t>(
            std::clamp(left, -0.95, 0.95) * 32767.0));
        output.samples.push_back(static_cast<int16_t>(
            std::clamp(right, -0.95, 0.95) * 32767.0));
    }
    return output;
}

} // namespace aur
