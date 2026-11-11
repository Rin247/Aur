#include "mini_test.hpp"
#include "aur/audio/StepSequencer.hpp"

#include "aur/audio/SongDocument.hpp"

#include "aur/audio/RealtimeMixer.hpp"

#include "aur/audio/synth.hpp"

#include <cmath>
#include <vector>

namespace
{
    using aur::Json;
    using aur::JsonArray;
    using aur::JsonObject;
    using aur::audio::StepCell;
    using aur::audio::StepEvent;
    using aur::audio::StepPattern;

    Json pattern_song(int length, int resolution, const JsonArray &steps)
    {
        JsonObject pattern{{"name", "Pattern 1"},
                           {"track", 0},
                           {"channel", 9},
                           {"program", 0},
                           {"length", length},
                           {"resolution", resolution},
                           {"seed", 7},
                           {"steps", steps}};
        return JsonObject{{"name", "Groove"},
                          {"tempo", 120},
                          {"bars", 8},
                          {"tracks", JsonArray{JsonObject{{"name", "Drums"},
                                                         {"channel", 9},
                                                         {"program", 0},
                                                         {"volume", 90},
                                                         {"pan", 64},
                                                         {"notes", JsonArray{}}}}},
                          {"step_patterns", JsonArray{pattern}}};
    }

    // A note is a nested array [step, pitch, length, velocity]; the braces
    // must wrap the inner array or it flattens into four numbers.
    Json make_note(int step, int pitch, int length, int velocity)
    {
        JsonArray fields;
        fields.emplace_back(step);
        fields.emplace_back(pitch);
        fields.emplace_back(length);
        fields.emplace_back(velocity);
        return Json(fields);
    }

    int preview_peak(const Json &song, int bars, int sampleRate = 8000)
    {
        aur::StereoPcm pcm = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, sampleRate, bars);
        int peak = 0;
        for (int16_t sample : pcm.samples)
            peak = std::max(peak, std::abs(static_cast<int>(sample)));
        return peak;
    }

    void test_pattern_read_defaults()
    {
        Json song = JsonObject{{"tracks", JsonArray{}}};
        ASSERT(aur::audio::read_step_patterns(song).empty());
        ASSERT(aur::audio::read_groove_settings(song).swing == 0);
        ASSERT(aur::audio::read_groove_settings(song).humanize == 0);
        ASSERT(aur::audio::read_groove_settings(song).groove == 0);
        ASSERT(aur::audio::compile_chain(song).empty());
    }

    void test_pattern_read_fields()
    {
        Json song = pattern_song(64, 4, JsonArray{JsonObject{{"step", 3},
                                                              {"pitch", 36},
                                                              {"velocity", 110},
                                                              {"accent", 1}}});
        const auto patterns = aur::audio::read_step_patterns(song);
        ASSERT(patterns.size() == 1);
        ASSERT(patterns[0].length == 64);
        ASSERT(patterns[0].resolution == 4);
        ASSERT(patterns[0].channel == 9);
        ASSERT(patterns[0].seed == 7);
        ASSERT(patterns[0].cells.size() == 1);
        ASSERT(patterns[0].cells[0].step == 3);
        ASSERT(patterns[0].cells[0].pitch == 36);
        ASSERT(patterns[0].cells[0].velocity == 110);
        ASSERT(patterns[0].cells[0].accent == 1);
    }

    void test_pattern_compile_steps()
    {
        Json song = pattern_song(16, 4, JsonArray{JsonObject{{"step", 0}, {"pitch", 36}},
                                                     JsonObject{{"step", 4}, {"pitch", 38}}});
        const auto events = aur::audio::compile_chain(song);
        ASSERT(events.size() == 2);
        ASSERT(events[0].step == 0.0);
        ASSERT(events[0].pitch == 36);
        ASSERT(events[1].step == 16.0);
        ASSERT(events[1].pitch == 38);
    }

    void test_pattern_accent_raises_velocity()
    {
        Json song = pattern_song(16, 4, JsonArray{JsonObject{{"step", 0},
                                                              {"pitch", 36},
                                                              {"velocity", 80},
                                                              {"accent", 1}}});
        const auto events = aur::audio::compile_chain(song);
        ASSERT(events.size() == 1);
        ASSERT(events[0].velocity > 80);
        ASSERT(events[0].velocity <= 127);
    }

    void test_pattern_ratchet_retriggers()
    {
        Json song = pattern_song(16, 4, JsonArray{JsonObject{{"step", 0},
                                                              {"pitch", 36},
                                                              {"ratchet", 3}}});
        const auto events = aur::audio::compile_chain(song);
        ASSERT(events.size() == 4); // 1 + 3 ratchets
        ASSERT(events[0].step < events[1].step);
        ASSERT(events[3].step < 4.0);
    }

    void test_pattern_swing_delays_odd_cells()
    {
        JsonArray steps{JsonObject{{"step", 0}, {"pitch", 36}},
                        JsonObject{{"step", 1}, {"pitch", 36}},
                        JsonObject{{"step", 2}, {"pitch", 36}}};
        Json song = pattern_song(16, 4, steps);
        song["groovebox"] = JsonObject{{"swing", 100}};
        const auto events = aur::audio::compile_chain(song);
        ASSERT(events.size() == 3);
        // The even cell is on the grid; the odd cell is delayed.
        ASSERT(std::fabs(events[0].step - 0.0) < 1e-9);
        ASSERT(events[1].step > events[0].step);
        ASSERT(std::fabs(events[1].step - 6.0) < 1e-9);
        ASSERT(std::fabs(events[2].step - 8.0) < 1e-9);
    }

    void test_pattern_humanize_is_deterministic()
    {
        JsonArray steps{JsonObject{{"step", 0}, {"pitch", 36}},
                        JsonObject{{"step", 2}, {"pitch", 36}}};
        Json song = pattern_song(16, 4, steps);
        song["groovebox"] = JsonObject{{"humanize", 100}};
        const auto first = aur::audio::compile_chain(song);
        const auto second = aur::audio::compile_chain(song);
        ASSERT(first.size() == second.size());
        for (size_t i = 0; i < first.size(); ++i)
            ASSERT(std::fabs(first[i].step - second[i].step) < 1e-12);

        // A different seed gives a different jitter.
        Json other = song;
        other["step_patterns"][0]["seed"] = 99;
        const auto third = aur::audio::compile_chain(other);
        bool differs = false;
        for (size_t i = 0; i < first.size() && i < third.size(); ++i)
            if (std::fabs(first[i].step - third[i].step) > 1e-9)
                differs = true;
        ASSERT(differs);
    }

    void test_pattern_chain_order()
    {
        JsonArray stepsA{JsonObject{{"step", 0}, {"pitch", 36}}};
        JsonArray stepsB{JsonObject{{"step", 0}, {"pitch", 40}}};
        Json song = pattern_song(16, 4, stepsA);
        song["step_patterns"].as_array().emplace_back(
            JsonObject{{"name", "Pattern 2"},
                       {"track", 0},
                       {"channel", 9},
                       {"length", 16},
                       {"resolution", 4},
                       {"seed", 7},
                       {"steps", stepsB}});
        // Without a chain every pattern plays once, in song order.
        auto events = aur::audio::compile_chain(song);
        ASSERT(events.size() == 2);
        ASSERT(events[0].pitch == 36);
        ASSERT(events[1].pitch == 40);
        ASSERT(events[1].step > events[0].step);

        // With a chain, the second pattern starts after the first ends.
        song["groovebox"] = JsonObject{{"chain", JsonArray{0, 1}}};
        events = aur::audio::compile_chain(song);
        ASSERT(events.size() == 2);
        ASSERT(events[0].step == 0.0);
        ASSERT(events[1].step == 64.0);
    }

    void test_song_document_pattern_accessors()
    {
        Json song = pattern_song(32, 4, JsonArray{JsonObject{{"step", 0}, {"pitch", 36}}});
        ASSERT(aur::audio::SongDocument::step_patterns(song).size() == 1);
        ASSERT(aur::audio::SongDocument::step_pattern_at(song, 0) != nullptr);
        ASSERT(aur::audio::SongDocument::step_pattern_at(song, 5) == nullptr);
        ASSERT(aur::audio::SongDocument::step_pattern_at(song, -1) == nullptr);

        const int index = aur::audio::SongDocument::add_step_pattern(song, "Second");
        ASSERT(index == 1);
        ASSERT(aur::audio::SongDocument::step_patterns(song).size() == 2);

        StepPattern edited;
        edited.name = "Edited";
        edited.length = 64;
        edited.resolution = 2;
        edited.cells.push_back(StepCell{5, 40, 100, 0, 0, 0, false});
        aur::audio::SongDocument::set_step_pattern(song, 1, edited);
        const auto patterns = aur::audio::SongDocument::step_patterns(song);
        ASSERT(patterns.size() == 2);
        ASSERT(patterns[1].name == "Edited");
        ASSERT(patterns[1].length == 64);
        ASSERT(patterns[1].resolution == 2);
        ASSERT(patterns[1].cells.size() == 1);
        ASSERT(patterns[1].cells[0].step == 5);

        aur::audio::SongDocument::remove_step_pattern(song, 1);
        ASSERT(aur::audio::SongDocument::step_patterns(song).size() == 1);
    }

    void test_song_document_groove_accessors()
    {
        Json song = JsonObject{{"tracks", JsonArray{}}};
        aur::audio::GrooveSettings groove;
        groove.swing = 40;
        groove.humanize = 15;
        groove.groove = -10;
        aur::audio::SongDocument::set_groove_settings(song, groove);
        const auto read = aur::audio::SongDocument::groove_settings(song);
        ASSERT(read.swing == 40);
        ASSERT(read.humanize == 15);
        ASSERT(read.groove == -10);

        aur::audio::SongDocument::set_groove_chain(song, {2, 0, 1});
        const auto chain = aur::audio::SongDocument::groove_chain(song);
        ASSERT(chain.size() == 3);
        ASSERT(chain[0] == 2);
        ASSERT(chain[2] == 1);
    }

    void test_pattern_song_renders_audible_output()
    {
        Json song = pattern_song(16, 4, JsonArray{JsonObject{{"step", 0},
                                                              {"pitch", 60},
                                                              {"velocity", 110}},
                                                     JsonObject{{"step", 8},
                                                              {"pitch", 60},
                                                              {"velocity", 110}}});
        int peak = preview_peak(song, 4);
        ASSERT(peak > 1000);
    }

    void test_realtime_mixer_renders_pattern()
    {
        Json song = pattern_song(16, 4, JsonArray{JsonObject{{"step", 0},
                                                              {"pitch", 60},
                                                              {"velocity", 110}}});
        aur::audio::RealtimeMixer mixer;
        mixer.setSampleRate(8000);
        mixer.resetPlayback();
        mixer.loadSong(song);
        std::vector<float> buffer(8000 * 2);
        mixer.render(buffer.data(), 8000);
        float peak = 0.f;
        for (float sample : buffer)
            peak = std::max(peak, std::fabs(sample));
        ASSERT(peak > 0.01f);
    }

    void test_mixer_without_patterns_is_unchanged()
    {
        // A song with no step_patterns key must render the same as before.
        JsonArray notes;
        notes.emplace_back(make_note(0, 60, 4, 100));
        JsonObject track{{"name", "Lead"}, {"channel", 0}, {"program", 0},
                         {"volume", 90}, {"pan", 64}, {"notes", notes}};
        JsonArray tracks;
        tracks.emplace_back(track);
        Json song = JsonObject{{"name", "Plain"},
                               {"tempo", 120},
                               {"bars", 1},
                               {"tracks", tracks}};
        int peak = preview_peak(song, 4);
        ASSERT(peak > 1000);
    }

    void test_pattern_humanize_is_deterministic_across_runs()
    {
        Json song = pattern_song(16, 4, JsonArray{JsonObject{{"step", 0},
                                                              {"pitch", 60},
                                                              {"velocity", 110}},
                                                     JsonObject{{"step", 4},
                                                              {"pitch", 62},
                                                              {"velocity", 90}}});
        Json pattern = song["step_patterns"][0];
        pattern["humanize_timing"] = 0.5;
        pattern["humanize_velocity"] = 0.5;
        aur::StereoPcm a = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 2);
        aur::StereoPcm b = aur::synthesize_preview(song, aur::PreviewSynth::MdFm, 8000, 2);
        ASSERT(a.samples.size() == b.samples.size());
        bool identical = true;
        for (size_t i = 0; i < a.samples.size(); ++i)
        {
            if (a.samples[i] != b.samples[i])
            {
                identical = false;
                break;
            }
        }
        ASSERT(identical);
    }
} // namespace

void test_step_sequencer()
{
    test_pattern_read_defaults();
    test_pattern_read_fields();
    test_pattern_compile_steps();
    test_pattern_accent_raises_velocity();
    test_pattern_ratchet_retriggers();
    test_pattern_swing_delays_odd_cells();
    test_pattern_humanize_is_deterministic();
    test_pattern_chain_order();
    test_song_document_pattern_accessors();
    test_song_document_groove_accessors();
    test_pattern_song_renders_audible_output();
    test_realtime_mixer_renders_pattern();
    test_mixer_without_patterns_is_unchanged();
    test_pattern_humanize_is_deterministic_across_runs();
}
