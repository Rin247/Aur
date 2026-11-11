#include "mini_test.hpp"
#include "aur/audio/Arpeggiator.hpp"
#include "aur/audio/SongDocument.hpp"

#include <algorithm>
#include <cmath>

namespace
{
    using aur::Json;
    using aur::JsonArray;
    using aur::JsonObject;
    using aur::audio::ArpMode;
    using aur::audio::ArpRate;
    using aur::audio::ArpSettings;

    void test_mode_and_rate_names()
    {
        ArpMode mode = ArpMode::Off;
        ASSERT(aur::audio::arp_mode_from_name("updown", mode));
        ASSERT(mode == ArpMode::UpDown);
        ASSERT(!aur::audio::arp_mode_from_name("nope", mode));

        ArpRate rate = ArpRate::Quarter;
        ASSERT(aur::audio::arp_rate_from_name("1/16T", rate));
        ASSERT(rate == ArpRate::SixteenthTriplet);
        ASSERT(!aur::audio::arp_rate_from_name("1/7", rate));

        ASSERT(std::string(aur::audio::arp_mode_name(ArpMode::AsPlayed)) == "asplayed");
        ASSERT(std::string(aur::audio::arp_rate_name(ArpRate::EighthTriplet)) == "1/8T");
    }

    void test_settings_roundtrip()
    {
        Json song = JsonObject{{"tracks", JsonArray{}}};
        ArpSettings settings;
        settings.mode = ArpMode::Up;
        settings.rate = ArpRate::Eighth;
        settings.gate = 55;
        settings.octave = 2;
        settings.seed = 42;
        aur::audio::write_arp_settings(song, settings);
        const auto read = aur::audio::read_arp_settings(song);
        ASSERT(read.mode == ArpMode::Up);
        ASSERT(read.rate == ArpRate::Eighth);
        ASSERT(read.gate == 55);
        ASSERT(read.octave == 2);
        ASSERT(read.seed == 42);
    }

    void test_settings_default_off()
    {
        Json song = JsonObject{{"tracks", JsonArray{}}};
        const auto read = aur::audio::read_arp_settings(song);
        ASSERT(read.mode == ArpMode::Off);
    }

    void test_arp_up_and_down()
    {
        ArpSettings settings;
        settings.mode = ArpMode::Up;
        auto pitches = aur::audio::arpeggiate({60, 64, 67}, settings);
        ASSERT(pitches.size() == 3);
        ASSERT(pitches[0] == 60);
        ASSERT(pitches[1] == 64);
        ASSERT(pitches[2] == 67);

        settings.mode = ArpMode::Down;
        pitches = aur::audio::arpeggiate({60, 64, 67}, settings);
        ASSERT(pitches.size() == 3);
        ASSERT(pitches[0] == 67);
        ASSERT(pitches[2] == 60);
    }

    void test_arp_updown_and_octave()
    {
        ArpSettings settings;
        settings.mode = ArpMode::UpDown;
        settings.octave = 1;
        auto pitches = aur::audio::arpeggiate({60, 64}, settings);
        // The octave lifts every base pitch, so the ladder is C E C' E';
        // up-down then returns to the bottom without repeating the apex.
        ASSERT(pitches.size() == 7);
        ASSERT(pitches[0] == 60);
        ASSERT(pitches[1] == 64);
        ASSERT(pitches[2] == 72);
        ASSERT(pitches[3] == 76);
        ASSERT(pitches[4] == 72);
        ASSERT(pitches[5] == 64);
        ASSERT(pitches[6] == 60);
    }

    void test_arp_chord_and_asplayed()
    {
        ArpSettings settings;
        settings.mode = ArpMode::Chord;
        auto pitches = aur::audio::arpeggiate({60, 64, 67}, settings);
        ASSERT(pitches.size() == 3);
        ASSERT(pitches[0] == 60);
        ASSERT(pitches[2] == 67);

        settings.mode = ArpMode::AsPlayed;
        pitches = aur::audio::arpeggiate({67, 60, 64}, settings);
        ASSERT(pitches.size() == 3);
        ASSERT(pitches[0] == 67);
        ASSERT(pitches[2] == 64);
    }

    void test_arp_random_is_deterministic()
    {
        ArpSettings settings;
        settings.mode = ArpMode::Random;
        settings.seed = 11;
        const auto first = aur::audio::arpeggiate({60, 62, 64, 65}, settings);
        const auto second = aur::audio::arpeggiate({60, 62, 64, 65}, settings);
        ASSERT(first.size() == second.size());
        for (size_t i = 0; i < first.size(); ++i)
            ASSERT(first[i] == second[i]);

        settings.seed = 12;
        const auto third = aur::audio::arpeggiate({60, 62, 64, 65}, settings);
        bool differs = false;
        for (size_t i = 0; i < first.size(); ++i)
            if (first[i] != third[i])
                differs = true;
        ASSERT(differs);
    }

    void test_arp_off_yields_nothing()
    {
        ArpSettings settings;
        settings.mode = ArpMode::Off;
        ASSERT(aur::audio::arpeggiate({60, 64}, settings).empty());
        ASSERT(aur::audio::arpeggiate_events({60, 64}, settings, 0.0, 1.0, 8).empty());
    }

    void test_arp_events_respect_gate()
    {
        ArpSettings settings;
        settings.mode = ArpMode::Up;
        settings.rate = ArpRate::Sixteenth;
        settings.gate = 50;
        const auto events = aur::audio::arpeggiate_events({60, 64, 67}, settings, 4.0, 1.0, 6);
        ASSERT(events.size() == 6);
        ASSERT(std::fabs(events[0].step - 4.0) < 1e-9);
        ASSERT(std::fabs(events[1].step - 5.0) < 1e-9);
        ASSERT(std::fabs(events[0].length - 0.5) < 1e-9);
        ASSERT(events[0].pitch == 60);
        ASSERT(events[1].pitch == 64);
        ASSERT(events[2].pitch == 67);
        ASSERT(events[3].pitch == 60);
    }
} // namespace

void test_arpeggiator()
{
    test_mode_and_rate_names();
    test_settings_roundtrip();
    test_settings_default_off();
    test_arp_up_and_down();
    test_arp_updown_and_octave();
    test_arp_chord_and_asplayed();
    test_arp_random_is_deterministic();
    test_arp_off_yields_nothing();
    test_arp_events_respect_gate();
}
