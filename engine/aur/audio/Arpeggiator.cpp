#include "aur/audio/Arpeggiator.hpp"
#include "aur/audio/StepSequencer.hpp"

#include <algorithm>
#include <cmath>

namespace aur::audio
{
    namespace
    {

        // One arp step in song steps (4 song steps per beat).
        double rate_in_steps(ArpRate rate)
        {
            switch (rate)
            {
            case ArpRate::Quarter: return 4.0;
            case ArpRate::Eighth: return 2.0;
            case ArpRate::EighthTriplet: return 4.0 / 3.0;
            case ArpRate::Sixteenth: return 1.0;
            case ArpRate::SixteenthTriplet: return 2.0 / 3.0;
            case ArpRate::ThirtySecond: return 0.5;
            }
            return 1.0;
        }

        std::uint32_t mix32(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

    } // namespace

    const char *arp_mode_name(ArpMode mode)
    {
        switch (mode)
        {
        case ArpMode::Off: return "off";
        case ArpMode::Up: return "up";
        case ArpMode::Down: return "down";
        case ArpMode::UpDown: return "updown";
        case ArpMode::Random: return "random";
        case ArpMode::Chord: return "chord";
        case ArpMode::AsPlayed: return "asplayed";
        }
        return "off";
    }

    const char *arp_rate_name(ArpRate rate)
    {
        switch (rate)
        {
        case ArpRate::Quarter: return "1/4";
        case ArpRate::Eighth: return "1/8";
        case ArpRate::EighthTriplet: return "1/8T";
        case ArpRate::Sixteenth: return "1/16";
        case ArpRate::SixteenthTriplet: return "1/16T";
        case ArpRate::ThirtySecond: return "1/32";
        }
        return "1/16";
    }

    bool arp_mode_from_name(const std::string &name, ArpMode &out)
    {
        static const struct { const char *name; ArpMode mode; } table[] = {
            {"off", ArpMode::Off}, {"up", ArpMode::Up}, {"down", ArpMode::Down},
            {"updown", ArpMode::UpDown}, {"random", ArpMode::Random},
            {"chord", ArpMode::Chord}, {"asplayed", ArpMode::AsPlayed}};
        for (const auto &row : table)
        {
            if (name == row.name)
            {
                out = row.mode;
                return true;
            }
        }
        return false;
    }

    bool arp_rate_from_name(const std::string &name, ArpRate &out)
    {
        static const struct { const char *name; ArpRate rate; } table[] = {
            {"1/4", ArpRate::Quarter}, {"1/8", ArpRate::Eighth},
            {"1/8T", ArpRate::EighthTriplet}, {"1/16", ArpRate::Sixteenth},
            {"1/16T", ArpRate::SixteenthTriplet}, {"1/32", ArpRate::ThirtySecond}};
        for (const auto &row : table)
        {
            if (name == row.name)
            {
                out = row.rate;
                return true;
            }
        }
        return false;
    }

    ArpSettings read_arp_settings(const Json &song)
    {
        ArpSettings settings;
        if (!song.is_object() || !song.contains("groovebox") ||
            !song["groovebox"].is_object())
            return settings;
        const Json &box = song["groovebox"];
        if (!box.contains("arp") || !box["arp"].is_object())
            return settings;
        const Json &arp = box["arp"];
        if (arp.contains("mode") && arp["mode"].is_string())
            arp_mode_from_name(arp["mode"].as_string(), settings.mode);
        if (arp.contains("rate") && arp["rate"].is_string())
            arp_rate_from_name(arp["rate"].as_string(), settings.rate);
        if (arp.contains("gate") && arp["gate"].is_number())
            settings.gate = std::clamp(arp["gate"].as_int(), 1, 100);
        if (arp.contains("octave") && arp["octave"].is_number())
            settings.octave = std::clamp(arp["octave"].as_int(), 0, 3);
        if (arp.contains("seed") && arp["seed"].is_number())
            settings.seed = arp["seed"].as_int();
        return settings;
    }

    void write_arp_settings(Json &song, const ArpSettings &settings)
    {
        if (!song.is_object())
            return;
        if (!song.contains("groovebox") || !song["groovebox"].is_object())
            song["groovebox"] = JsonObject{};
        Json &box = song["groovebox"];
        box["arp"] = JsonObject{
            {"mode", std::string(arp_mode_name(settings.mode))},
            {"rate", std::string(arp_rate_name(settings.rate))},
            {"gate", settings.gate},
            {"octave", settings.octave},
            {"seed", settings.seed}};
    }

    std::vector<int> arpeggiate(const std::vector<int> &pitches,
                                const ArpSettings &settings)
    {
        std::vector<int> out;
        if (pitches.empty() || settings.mode == ArpMode::Off)
            return out;
        std::vector<int> base = pitches;
        std::sort(base.begin(), base.end());
        base.erase(std::unique(base.begin(), base.end()), base.end());
        if (base.empty())
            return out;

        if (settings.mode == ArpMode::Chord)
        {
            out = base;
            for (int octave = 1; octave <= settings.octave; ++octave)
            {
                for (int pitch : base)
                {
                    const int shifted = pitch + octave * 12;
                    if (shifted <= 127)
                        out.push_back(shifted);
                }
            }
            return out;
        }

        std::vector<int> ladder = base;
        for (int octave = 1; octave <= settings.octave; ++octave)
        {
            for (int pitch : base)
            {
                const int shifted = pitch + octave * 12;
                if (shifted <= 127)
                    ladder.push_back(shifted);
            }
        }
        if (ladder.empty())
            return out;

        switch (settings.mode)
        {
        case ArpMode::Up:
            out = ladder;
            break;
        case ArpMode::Down:
            out.assign(ladder.rbegin(), ladder.rend());
            break;
        case ArpMode::UpDown:
        {
            // Up the ladder, then back down without repeating the apex.
            out = ladder;
            for (size_t i = ladder.size() - 1; i-- > 0;)
                out.push_back(ladder[i]);
            break;
        }
        case ArpMode::Random:
        {
            // A deterministic permutation per seed, so a reroll is
            // reproducible (contract 5).
            const std::uint32_t hash = mix32(static_cast<std::uint32_t>(settings.seed) * 0x9e3779b9u);
            for (size_t i = 0; i < ladder.size(); ++i)
            {
                const std::uint32_t pick = mix32(hash + static_cast<std::uint32_t>(i));
                out.push_back(ladder[pick % ladder.size()]);
            }
            break;
        }
        case ArpMode::AsPlayed:
            out = pitches;
            break;
        case ArpMode::Off:
        case ArpMode::Chord:
            break;
        }
        return out;
    }

    std::vector<ArpEvent> arpeggiate_events(const std::vector<int> &pitches,
                                             const ArpSettings &settings,
                                             double startStep,
                                             double stepLength,
                                             int count)
    {
        std::vector<ArpEvent> events;
        if (settings.mode == ArpMode::Off || count <= 0)
            return events;
        const std::vector<int> order = arpeggiate(pitches, settings);
        if (order.empty())
            return events;
        const double gate = std::clamp(settings.gate, 1, 100) / 100.0;
        for (int i = 0; i < count; ++i)
        {
            ArpEvent event;
            event.step = startStep + stepLength * i;
            event.pitch = order[static_cast<size_t>(i) % order.size()];
            event.velocity = 90;
            event.length = stepLength * gate;
            events.push_back(event);
        }
        return events;
    }

} // namespace aur::audio
