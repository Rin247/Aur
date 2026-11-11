#include "aur/audio/StepSequencer.hpp"

#include <algorithm>
#include <cmath>

namespace aur::audio
{
    namespace
    {

        int int_of(const Json &value, int fallback)
        {
            return value.is_number() ? value.as_int() : fallback;
        }

        int clamp_int(int value, int low, int high)
        {
            return std::clamp(value, low, high);
        }

        // A cheap integer hash; the mixer must not pull in <random> on the
        // audio thread, and this only needs to be stable, not cryptographic.
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

    double humanize_offset(int seed, int cell)
    {
        const std::uint32_t hash = mix32(static_cast<std::uint32_t>(seed) * 0x9e3779b9u ^
                                          static_cast<std::uint32_t>(cell) * 0x85ebca6bu);
        // Map to [-1, 1).
        return static_cast<double>(hash % 20001) / 10000.0 - 1.0;
    }

    std::vector<StepPattern> read_step_patterns(const Json &song)
    {
        std::vector<StepPattern> out;
        if (!song.is_object() || !song.contains("step_patterns") ||
            !song["step_patterns"].is_array())
            return out;
        for (const Json &entry : song["step_patterns"].as_array())
        {
            if (!entry.is_object())
                continue;
            StepPattern pattern;
            if (entry.contains("name") && entry["name"].is_string())
                pattern.name = entry["name"].as_string();
            pattern.track = clamp_int(int_of(entry.contains("track") ? entry["track"] : Json{}, 0),
                                      0, 255);
            pattern.channel = clamp_int(int_of(entry.contains("channel") ? entry["channel"] : Json{}, 0),
                                         0, 15);
            pattern.program = clamp_int(int_of(entry.contains("program") ? entry["program"] : Json{}, 0),
                                        0, 127);
            pattern.length = clamp_int(int_of(entry.contains("length") ? entry["length"] : Json{}, 128),
                                       1, 128);
            pattern.resolution = clamp_int(int_of(entry.contains("resolution") ? entry["resolution"] : Json{}, 4),
                                           1, 16);
            pattern.seed = int_of(entry.contains("seed") ? entry["seed"] : Json{}, 1);
            if (entry.contains("steps") && entry["steps"].is_array())
            {
                for (const Json &cell : entry["steps"].as_array())
                {
                    if (!cell.is_object() || !cell.contains("step") || !cell["step"].is_number())
                        continue;
                    StepCell step;
                    step.step = clamp_int(cell["step"].as_int(), 0, 127);
                    step.pitch = clamp_int(int_of(cell.contains("pitch") ? cell["pitch"] : Json{}, 60),
                                           0, 127);
                    step.velocity = clamp_int(int_of(cell.contains("velocity") ? cell["velocity"] : Json{}, 90),
                                              0, 127);
                    step.accent = clamp_int(int_of(cell.contains("accent") ? cell["accent"] : Json{}, 0),
                                            0, 1);
                    step.slide = clamp_int(int_of(cell.contains("slide") ? cell["slide"] : Json{}, 0),
                                           -24, 24);
                    step.ratchet = clamp_int(int_of(cell.contains("ratchet") ? cell["ratchet"] : Json{}, 0),
                                             0, 8);
                    step.tie = cell.contains("tie") && cell["tie"].is_bool() && cell["tie"].as_bool();
                    pattern.cells.push_back(step);
                }
                std::stable_sort(pattern.cells.begin(), pattern.cells.end(),
                                 [](const StepCell &a, const StepCell &b)
                                 { return a.step < b.step; });
            }
            out.push_back(std::move(pattern));
        }
        return out;
    }

    GrooveSettings read_groove_settings(const Json &song)
    {
        GrooveSettings groove;
        if (!song.is_object() || !song.contains("groovebox") || !song["groovebox"].is_object())
            return groove;
        const Json &box = song["groovebox"];
        groove.swing = clamp_int(int_of(box.contains("swing") ? box["swing"] : Json{}, 0), 0, 100);
        groove.humanize = clamp_int(int_of(box.contains("humanize") ? box["humanize"] : Json{}, 0), 0, 100);
        groove.groove = clamp_int(int_of(box.contains("groove") ? box["groove"] : Json{}, 0), -50, 50);
        return groove;
    }

    std::vector<StepEvent> compile_pattern(const StepPattern &pattern,
                                            const GrooveSettings &groove)
    {
        std::vector<StepEvent> events;
        const int resolution = std::clamp(pattern.resolution, 1, 16);
        const double swingFraction = groove.swing / 100.0 * 0.5;
        const double humanizeFraction = groove.humanize / 100.0 * 0.5;

        for (const StepCell &cell : pattern.cells)
        {
            if (cell.step < 0 || cell.step >= pattern.length)
                continue;
            const double base = static_cast<double>(cell.step) * resolution;
            // Swing delays the odd cells of a bar (a 16th-note grid).
            double step = base;
            if (resolution <= 4)
            {
                const int cellInBar = cell.step % (16 / std::max(1, resolution));
                const int swingPeriod = 16 / std::max(1, resolution);
                if (swingPeriod > 0 && (cellInBar % 2) == 1)
                    step += swingFraction * resolution;
            }
            // Humanise jitters the cell by a seeded amount of half a cell.
            step += humanize_offset(pattern.seed, cell.step) * humanizeFraction * resolution;

            int velocity = cell.velocity;
            if (cell.accent)
                velocity = std::clamp(velocity + 25, 0, 127);
            velocity = std::clamp(velocity + groove.groove, 0, 127);

            const int retriggers = 1 + cell.ratchet;
            const double cellLength = resolution;
            const double noteLength = cell.tie
                                         ? cellLength * 2.0
                                         : cellLength * 0.9;
            for (int i = 0; i < retriggers; ++i)
            {
                StepEvent event;
                event.step = step + (cellLength * i / retriggers);
                event.pitch = cell.pitch;
                event.velocity = velocity;
                event.length = noteLength / retriggers;
                event.track = pattern.track;
                event.channel = pattern.channel;
                event.program = pattern.program;
                events.push_back(event);
            }
            if (cell.slide != 0)
            {
                // A slide starts a step below and glides into the cell pitch.
                StepEvent slide;
                slide.step = std::max(0.0, step - resolution * 0.5);
                slide.pitch = std::clamp(cell.pitch - cell.slide, 0, 127);
                slide.velocity = velocity;
                slide.length = resolution * 0.5;
                slide.track = pattern.track;
                slide.channel = pattern.channel;
                slide.program = pattern.program;
                events.push_back(slide);
            }
        }
        std::stable_sort(events.begin(), events.end(),
                         [](const StepEvent &a, const StepEvent &b)
                         { return a.step < b.step; });
        return events;
    }

    std::vector<StepEvent> compile_chain(const Json &song)
    {
        const auto patterns = read_step_patterns(song);
        std::vector<StepEvent> events;
        if (patterns.empty())
            return events;
        const GrooveSettings groove = read_groove_settings(song);

        std::vector<int> order;
        if (song.contains("groovebox") && song["groovebox"].is_object() &&
            song["groovebox"].contains("chain") && song["groovebox"]["chain"].is_array())
        {
            for (const Json &index : song["groovebox"]["chain"].as_array())
            {
                if (index.is_number())
                    order.push_back(index.as_int());
            }
        }
        if (order.empty())
        {
            for (int i = 0; i < static_cast<int>(patterns.size()); ++i)
                order.push_back(i);
        }

        // The chain plays its patterns one after the other; each pattern
        // advances the chain by its own length so the bars line up.
        double chainStep = 0.0;
        for (int index : order)
        {
            if (index < 0 || index >= static_cast<int>(patterns.size()))
                continue;
            const StepPattern &pattern = patterns[static_cast<size_t>(index)];
            for (StepEvent event : compile_pattern(pattern, groove))
            {
                event.step += chainStep;
                events.push_back(event);
            }
            chainStep += static_cast<double>(pattern.length) * pattern.resolution;
        }
        std::stable_sort(events.begin(), events.end(),
                         [](const StepEvent &a, const StepEvent &b)
                         { return a.step < b.step; });
        return events;
    }

} // namespace aur::audio
