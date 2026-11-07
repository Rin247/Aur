#include "aur/audio/VoiceCore.hpp"

#include <algorithm>
#include <cmath>

namespace aur
{
    namespace audio
    {
        namespace
        {

            constexpr double pi = 3.14159265358979323846;

        } // namespace

        double voice_envelope(const Voice &voice, std::int64_t frame, int sample_rate)
        {
            const double attack = std::max(1, sample_rate / 200);
            const double release = std::max(1, sample_rate / 8);
            const double attack_gain = std::clamp(
                static_cast<double>(frame - voice.start) / attack, 0.0, 1.0);
            const double release_gain = std::clamp(
                static_cast<double>(voice.end - frame) / release, 0.0, 1.0);
            return attack_gain * release_gain;
        }

        double render_voice_sample(Voice &voice, Timbre timbre,
                                   double time, std::int64_t frame, int sample_rate)
        {
            if (timbre == Timbre::Fm)
            {
                const int algorithm = voice.program % 4;
                double operators[6]{};
                for (int op = 0; op < 6; ++op)
                {
                    const double ratio = 0.5 + ((voice.program * (op + 3) + op * 7) % 16) * 0.25;
                    const double decay = std::exp(-time * (0.6 + ((voice.program + op * 5) % 8) * 0.28));
                    const double level = 0.32 + ((voice.program + op * 11) % 9) * 0.11;
                    const double phase_increment = 2.0 * pi * voice.frequency * ratio / sample_rate;
                    voice.phase[op] = std::fmod(voice.phase[op] + phase_increment, 2.0 * pi);
                    const double feedback = (0.35 + (voice.program % 5) * 0.12) * decay;
                    operators[op] = std::sin(voice.phase[op] + feedback *
                                                                   (op == 0 ? 0.0 : operators[op - 1])) *
                                    level * decay;
                }

                switch (algorithm)
                {
                case 0:
                    return std::sin(voice.phase[5] + operators[4] * 4.0);
                case 1:
                    return (std::sin(voice.phase[5] + operators[4] * 3.0) +
                            std::sin(voice.phase[3] + operators[2] * 3.0) +
                            std::sin(voice.phase[1] + operators[0] * 2.0)) /
                           3.0;
                case 2:
                    return (std::sin(voice.phase[5] + operators[4] * 2.2 +
                                     operators[2] * 1.8 + operators[0] * 1.3) +
                            std::sin(voice.phase[3] + operators[1] * 1.7)) *
                           0.5;
                default:
                    return (std::sin(voice.phase[0]) + std::sin(voice.phase[1]) +
                            std::sin(voice.phase[2]) + std::sin(voice.phase[3]) +
                            std::sin(voice.phase[4]) + std::sin(voice.phase[5])) /
                           6.0;
                }
            }

            if (timbre == Timbre::RetroPsg)
            {
                const double phase = voice.phase[0] / (2.0 * pi);
                double sample = 0.0;
                switch (voice.program % 4)
                {
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

            const int family = voice.program / 8;
            const double phase = voice.phase[0] / (2.0 * pi);
            const double brightness = 0.42 + (voice.program % 8) * 0.065;
            double sample = 0.0;
            if (voice.drum)
            {
                std::uint32_t noise_state = static_cast<std::uint32_t>(frame) ^
                                            static_cast<std::uint32_t>(voice.program * 0x9e3779b9u) ^
                                            static_cast<std::uint32_t>(std::lround(voice.frequency * 31.0));
                noise_state ^= noise_state << 13;
                noise_state ^= noise_state >> 17;
                noise_state ^= noise_state << 5;
                const double noise = static_cast<double>(noise_state & 0xffffu) / 32767.5 - 1.0;
                if (voice.pitch <= 36)
                {
                    const double kick_frequency = 52.0 + 74.0 * std::exp(-time * 18.0);
                    sample = std::sin(voice.phase[0]) * std::exp(-time * 5.2) +
                             noise * std::exp(-time * 35.0) * 0.22;
                    voice.phase[0] = std::fmod(
                        voice.phase[0] + 2.0 * pi * kick_frequency / sample_rate, 2.0 * pi);
                }
                else if (voice.pitch == 42 || voice.pitch == 44 || voice.pitch == 46)
                {
                    sample = noise * std::exp(-time * (voice.pitch == 46 ? 5.0 : 13.0));
                }
                else
                {
                    sample = (noise * 0.68 + std::sin(voice.phase[0]) * 0.32) *
                             std::exp(-time * (voice.pitch <= 40 ? 7.0 : 5.0));
                    voice.phase[0] = std::fmod(
                        voice.phase[0] + 2.0 * pi * voice.frequency / sample_rate, 2.0 * pi);
                }
                return sample;
            }

            voice.phase[0] = std::fmod(
                voice.phase[0] + 2.0 * pi * voice.frequency / sample_rate, 2.0 * pi);
            switch (family)
            {
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
                for (int harmonic = 1; harmonic <= 6; ++harmonic)
                {
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
                                                 : (phase < 0.5 ? 1.0 : -1.0)) *
                             0.46 +
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
            if (family == 5 || family == 6 || family == 11)
                attack = 0.12;
            else if (family == 7 || family == 8)
                attack = 0.035;
            else if (family == 2 || family == 9)
                attack = 0.018;
            else if (family == 10 || family == 12)
                attack = 0.025;
            if (family == 0)
                decay = 1.1 + (voice.program % 4) * 0.35;
            else if (family == 3 || family == 4 || family == 13)
                decay = 2.2;
            else if (family == 1 || family == 14)
                decay = 3.8;
            const double attack_gain = std::clamp(time / attack, 0.0, 1.0);
            const double decay_gain = decay > 0.0
                                          ? 0.42 + 0.58 * std::exp(-time * decay)
                                          : 1.0;
            return sample * attack_gain * decay_gain;
        }

        Voice voice_from_note(const Json &track, const Json &note, int bpm,
                              int sample_rate, std::int64_t startFrame)
        {
            (void)bpm;
            Voice voice;
            voice.program = track.contains("program") && track["program"].is_number()
                                ? std::clamp(track["program"].as_int(), 0, 127)
                                : 0;
            voice.pitch = std::clamp(note[1].as_int(), 0, 127);
            voice.frequency = 440.0 * std::pow(2.0, (voice.pitch - 69) / 12.0);
            voice.drum = (track.contains("channel") && track["channel"].is_number() &&
                          track["channel"].as_int() == 9) ||
                         (track.contains("drum") && track["drum"].is_bool() &&
                          track["drum"].as_bool());
            (void)startFrame;
            return voice;
        }

    } // namespace audio
} // namespace aur
