#include "mini_test.hpp"
#include "aur/audio/midi.hpp"

#include "aur/audio/synth.hpp"

#include <cstdint>
#include <stdexcept>

#include <string>

#include <vector>

namespace {

std::vector<std::uint8_t> make_midi_with_division(std::uint16_t division) {
    const auto append_u16 = [](std::vector<std::uint8_t>& bytes, std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        bytes.push_back(static_cast<std::uint8_t>(value & 0xff));
    };
    const auto append_vlq = [](std::vector<std::uint8_t>& bytes, std::uint32_t value) {
        std::vector<std::uint8_t> encoded{static_cast<std::uint8_t>(value & 0x7f)};
        while ((value >>= 7) != 0) {
            encoded.insert(encoded.begin(), static_cast<std::uint8_t>((value & 0x7f) | 0x80));
        }
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    };
    const auto append_u32 = [](std::vector<std::uint8_t>& bytes, std::uint32_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value >> 24));
        bytes.push_back(static_cast<std::uint8_t>((value >> 16) & 0xff));
        bytes.push_back(static_cast<std::uint8_t>((value >> 8) & 0xff));
        bytes.push_back(static_cast<std::uint8_t>(value & 0xff));
    };
    std::vector<std::uint8_t> track{0x00, 0x90, 60, 100};
    append_vlq(track, division / 2);
    track.insert(track.end(), {0x80, 60, 0, 0x00, 0xff, 0x2f, 0x00});
    std::vector<std::uint8_t> midi{'M', 'T', 'h', 'd'};
    append_u32(midi, 6);
    append_u16(midi, 0);
    append_u16(midi, 1);
    append_u16(midi, division);
    midi.insert(midi.end(), {'M', 'T', 'r', 'k'});
    append_u32(midi, static_cast<std::uint32_t>(track.size()));
    midi.insert(midi.end(), track.begin(), track.end());
    return midi;
}

} // namespace

// P4: MIDI fidelity — tempo/T.S. maps, markers, CC/bend/pressure automation
// survive a full export/import round trip on the v2 model.
void test_midi_fidelity()
{
    auto point = [](int step, int value) {
        return aur::JsonObject{{"step", step}, {"value", value}};
    };
    auto lane = [](int cc, aur::JsonArray points) {
        return aur::JsonObject{{"cc", cc}, {"points", std::move(points)}};
    };
    aur::Json song = aur::JsonObject{
        {"name", "Fidelity"},
        {"tempo", 100}, {"beats", 4},
        {"tempo_map", aur::JsonArray{
            aur::JsonObject{{"step", 0}, {"bpm", 100}},
            aur::JsonObject{{"step", 16}, {"bpm", 150}}}},
        {"time_sig_map", aur::JsonArray{
            aur::JsonObject{{"step", 0}, {"num", 4}, {"den", 4}},
            aur::JsonObject{{"step", 32}, {"num", 7}, {"den", 8}}}},
        {"markers", aur::JsonArray{
            aur::JsonObject{{"step", 16}, {"name", "Chorus"}}}},
        {"tracks", aur::JsonArray{
            aur::JsonObject{
                {"name", "Lead"}, {"channel", 1}, {"program", 40},
                {"automation", aur::JsonArray{
                    lane(74, aur::JsonArray{point(0, 40), point(8, 100)}),
                    lane(128, aur::JsonArray{point(4, 1024), point(12, -2048)}),
                    lane(129, aur::JsonArray{point(2, 64)})}},
                {"notes", aur::JsonArray{aur::JsonArray{0, 60, 4, 100}}}}}}};

    const aur::Json imported = aur::import_standard_midi(
        aur::export_standard_midi(song), "X");

    // Tempo map: both entries round-trip (sorted, first becomes scalar tempo).
    const aur::JsonArray& tempo_map = imported["tempo_map"].as_array();
    ASSERT(tempo_map.size() == 2);
    ASSERT(tempo_map[0]["step"].as_int() == 0 && tempo_map[0]["bpm"].as_int() == 100);
    ASSERT(tempo_map[1]["step"].as_int() == 16 && tempo_map[1]["bpm"].as_int() == 150);
    ASSERT(imported["tempo"].as_int() == 100);

    // Time-signature map round-trips including 7/8.
    const aur::JsonArray& sig_map = imported["time_sig_map"].as_array();
    ASSERT(sig_map.size() == 2);
    ASSERT(sig_map[1]["num"].as_int() == 7 && sig_map[1]["den"].as_int() == 8);

    // Marker round-trips with name and step.
    ASSERT(imported["markers"].as_array().size() == 1);
    ASSERT(imported["markers"][0]["name"].as_string() == "Chorus");
    ASSERT(imported["markers"][0]["step"].as_int() == 16);

    // Automation: CC74, bend (±1024/−2048 -> 14-bit), pressure all return.
    const aur::Json& lead = imported["tracks"][0];
    ASSERT(lead.contains("automation") && lead["automation"].is_array());
    const aur::JsonArray& lanes = lead["automation"].as_array();
    ASSERT(lanes.size() == 3);
    bool saw_cc74 = false, saw_bend = false, saw_pressure = false;
    for (const aur::Json& lane : lanes) {
        const int cc = lane["cc"].as_int();
        if (cc == 74) {
            saw_cc74 = true;
            ASSERT(lane["points"].as_array().size() == 2);
            ASSERT(lane["points"][0]["value"].as_int() == 40);
            ASSERT(lane["points"][1]["value"].as_int() == 100);
        } else if (cc == 128) {
            saw_bend = true;
            const aur::JsonArray& points = lane["points"].as_array();
            ASSERT(points.size() == 2);
            ASSERT(points[0]["value"].as_int() == 1024);
            ASSERT(points[1]["value"].as_int() == -2048);
        } else if (cc == 129) {
            saw_pressure = true;
            ASSERT(lane["points"][0]["value"].as_int() == 64);
        }
    }
    ASSERT(saw_cc74 && saw_bend && saw_pressure);

    // Import-side synthetic file: CC + bend + marker inside a hand-built track.
    std::vector<std::uint8_t> track{
        0x00, 0xff, 0x06, 5, 'B', 'e', 'g', 'i', 'n',
        0x00, 0xb0, 11, 80,
        0x00, 0xe0, 0x00, 0x50,
        0x00, 0xd0, 70,
        0x00, 0x90, 62, 100,
        0x60, 0x80, 62, 0,
        0x00, 0xff, 0x51, 3, 0x07, 0xa1, 0x20,
        0x00, 0xff, 0x2f, 0x00};
    std::vector<std::uint8_t> midi{'M', 'T', 'h', 'd'};
    const auto append_u32 = [&](std::uint32_t v) {
        midi.push_back(static_cast<std::uint8_t>(v >> 24));
        midi.push_back(static_cast<std::uint8_t>(v >> 16));
        midi.push_back(static_cast<std::uint8_t>(v >> 8));
        midi.push_back(static_cast<std::uint8_t>(v));
    };
    const auto append_u16 = [&](std::uint16_t v) {
        midi.push_back(static_cast<std::uint8_t>(v >> 8));
        midi.push_back(static_cast<std::uint8_t>(v));
    };
    append_u32(6); append_u16(0); append_u16(1); append_u16(480);
    midi.insert(midi.end(), {'M', 'T', 'r', 'k'});
    append_u32(static_cast<std::uint32_t>(track.size()));
    midi.insert(midi.end(), track.begin(), track.end());
    const aur::Json synthetic = aur::import_standard_midi(midi, "Synth");
    ASSERT(synthetic["markers"][0]["name"].as_string() == "Begin");
    const aur::Json& synth_track = synthetic["tracks"][0];
    ASSERT(synth_track.contains("automation"));
    bool saw_cc11 = false, saw_bend_lane = false, saw_pressure_lane = false;
    int tempo_entries = 0;
    for (const aur::Json& lane : synth_track["automation"].as_array()) {
        if (lane["cc"].as_int() == 11) { saw_cc11 = true; ASSERT(lane["points"][0]["value"].as_int() == 80); }
        else if (lane["cc"].as_int() == 128) { saw_bend_lane = true; ASSERT(lane["points"][0]["value"].as_int() == 2048); }
        else if (lane["cc"].as_int() == 129) { saw_pressure_lane = true; ASSERT(lane["points"][0]["value"].as_int() == 70); }
    }
    ASSERT(saw_cc11 && saw_bend_lane && saw_pressure_lane);
    for (const aur::Json& point : synthetic["tempo_map"].as_array())
        ++tempo_entries;
    ASSERT(tempo_entries >= 1);
}

    void assert_ppqn_divisions()
    {
        for (const std::uint16_t division : {96, 384, 480, 960}) {
            const aur::Json ppqn_song = aur::import_standard_midi(
                make_midi_with_division(division), "PPQN fixture");
            ASSERT(ppqn_song["ticks_per_quarter"].as_int() == division);
            ASSERT(ppqn_song["tracks"][0]["notes"][0][0].as_int() == 0);
            ASSERT(ppqn_song["tracks"][0]["notes"][0][2].as_int() == 2);
        }
    }

void test_midi_import_export() {
    assert_ppqn_divisions();
    aur::Json song = aur::JsonObject{
        {"name", "Aur MIDI"},
        {"tempo", 132},
        {"beats", 3},
        {"time_signature", aur::JsonArray{3, 4}},
        {"sysex", aur::Json(aur::JsonArray{
            aur::Json(aur::JsonArray{
                0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7
            })
        })},
        {"tracks", aur::JsonArray{
            aur::JsonObject{
                {"name", "FM Lead"}, {"channel", 2}, {"program", 81},
                {"bank", 256}, {"volume", 96}, {"pan", 48},
                {"notes", aur::JsonArray{
                    aur::JsonArray{0, 64, 4, 110},
                    aur::JsonArray{8, 67, 6, 90}
                }}
            },
            aur::JsonObject{
                {"name", "Drums"}, {"channel", 9}, {"program", 0},
                {"bank", 0}, {"volume", 100}, {"pan", 64},
                {"drum", true}, {"notes", aur::JsonArray{aur::JsonArray{0, 36, 2, 100}}}
            }
        }}
    };

    assert_ppqn_divisions();

    const std::vector<uint8_t> bytes = aur::export_standard_midi(song);
    ASSERT(bytes.size() > 64);
    ASSERT(bytes[0] == 'M' && bytes[1] == 'T' && bytes[2] == 'h' && bytes[3] == 'd');
    const aur::Json imported = aur::import_standard_midi(bytes, "Fallback");
    ASSERT(imported["name"].as_string() == "Aur MIDI");
    ASSERT(imported["tempo"].as_int() == 132);
    ASSERT(imported["beats"].as_int() == 3);
    ASSERT(imported["time_signature"][1].as_int() == 4);
    ASSERT(imported["tracks"].as_array().size() == 2);
    ASSERT(imported["tracks"][0]["channel"].as_int() == 2);
    ASSERT(imported["tracks"][0]["program"].as_int() == 81);
    ASSERT(imported["tracks"][0]["bank"].as_int() == 256);
    ASSERT(imported["tracks"][0]["volume"].as_int() == 96);
    ASSERT(imported["tracks"][0]["pan"].as_int() == 48);
    ASSERT(imported["tracks"][0]["notes"].as_array().size() == 2);
    ASSERT(imported["tracks"][0]["notes"][0][0].as_int() == 0);
    ASSERT(imported["tracks"][0]["notes"][0][1].as_int() == 64);
    ASSERT(imported["tracks"][0]["notes"][0][2].as_int() == 4);
    ASSERT(imported["tracks"][0]["notes"][0][3].as_int() == 110);
    ASSERT(imported["tracks"][1]["channel"].as_int() == 9);
    ASSERT(imported["tracks"][1]["drum"].as_bool());
    ASSERT(imported["sysex"].as_array().size() == 1);
    ASSERT(imported["sysex"][0][1].as_int() == 0x41);

    const std::vector<uint8_t> round_trip = aur::export_standard_midi(imported);
    const aur::Json imported_again = aur::import_standard_midi(round_trip);
    ASSERT(imported_again["tracks"].as_array().size() == 2);
    ASSERT(imported_again["tracks"][0]["notes"][1][1].as_int() == 67);
    ASSERT(imported_again["sysex"].as_array().size() == 1);
    const aur::StereoPcm fm = aur::synthesize_preview(
        imported, aur::PreviewSynth::MdFm, 8000, 10);
    const aur::StereoPcm psg = aur::synthesize_preview(
        imported, aur::PreviewSynth::Spc, 8000, 10);
    ASSERT(fm.sample_rate == 8000);
    ASSERT(fm.samples.size() == psg.samples.size());
    ASSERT(!fm.samples.empty());
    ASSERT(fm.samples != psg.samples);
    aur::Json alternateProgram = imported;
    alternateProgram["tracks"][0]["program"] = 0;
    const aur::StereoPcm alternateFm = aur::synthesize_preview(
        alternateProgram, aur::PreviewSynth::MdFm, 8000, 10);
    ASSERT(fm.samples != alternateFm.samples);
    bool bad_file_rejected = false;
    try {
        (void)aur::import_standard_midi({0, 1, 2});
    } catch (const std::runtime_error&) {
        bad_file_rejected = true;
    }
    ASSERT(bad_file_rejected);
}
