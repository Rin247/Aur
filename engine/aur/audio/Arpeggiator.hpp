// Arpeggiator.hpp - chord/pitch expansion into a note stream.
// Qt-free. The arpeggiator compiles to note events like the step
// sequencer does, so the offline render and the live mixer agree exactly.
#pragma once

#include <string>
#include <vector>
#include "aur/json.hpp"

namespace aur::audio
{

    enum class ArpMode
    {
        Off = 0,
        Up,
        Down,
        UpDown,
        Random,
        Chord,
        AsPlayed
    };

    enum class ArpRate
    {
        Quarter = 0,   // 1/4
        Eighth,        // 1/8
        EighthTriplet, // 1/8T
        Sixteenth,     // 1/16
        SixteenthTriplet,
        ThirtySecond
    };

    struct ArpSettings
    {
        ArpMode mode = ArpMode::Off;
        ArpRate rate = ArpRate::Sixteenth;
        int gate = 80;      // 1..100, note length as a percent of the step
        int octave = 0;     // 0..3, extra octaves above the held chord
        int seed = 1;       // deterministic seed for the random mode
    };

    // Human-readable mode names for the UI and the song document.
    const char *arp_mode_name(ArpMode mode);
    const char *arp_rate_name(ArpRate rate);
    bool arp_mode_from_name(const std::string &name, ArpMode &out);
    bool arp_rate_from_name(const std::string &name, ArpRate &out);

    // Reads song["groovebox"]["arp"] into settings. A song without the key
    // reads as the arpeggiator off.
    ArpSettings read_arp_settings(const Json &song);

    // Writes the settings back into song["groovebox"]["arp"].
    void write_arp_settings(Json &song, const ArpSettings &settings);

    // Expands one chord (the held pitches, lowest first) into the pitch
    // order the mode produces. Chord mode returns the pitches unchanged.
    std::vector<int> arpeggiate(const std::vector<int> &pitches, const ArpSettings &settings);

    // Expands a chord into full note events over a step span. stepLength is
    // the length of one arp step in song steps; the events start at startStep.
    struct ArpEvent
    {
        double step = 0.0;
        int pitch = 60;
        int velocity = 90;
        double length = 1.0;
    };
    std::vector<ArpEvent> arpeggiate_events(const std::vector<int> &pitches,
                                             const ArpSettings &settings,
                                             double startStep,
                                             double stepLength,
                                             int count);

} // namespace aur::audio
