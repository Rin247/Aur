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
void test_midi_import_export() {
    for (const std::uint16_t division : {96, 384, 480, 960}) {
        const aur::Json ppqn_song = aur::import_standard_midi(
            make_midi_with_division(division), "PPQN fixture");
        ASSERT(ppqn_song["ticks_per_quarter"].as_int() == division);
        ASSERT(ppqn_song["tracks"][0]["notes"][0][0].as_int() == 0);
        ASSERT(ppqn_song["tracks"][0]["notes"][0][2].as_int() == 2);
    }
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

    for (const std::uint16_t division : {96, 384, 480, 960}) {
        const aur::Json ppqn_song = aur::import_standard_midi(
            make_midi_with_division(division), "PPQN fixture");
        ASSERT(ppqn_song["ticks_per_quarter"].as_int() == division);
        ASSERT(ppqn_song["tracks"][0]["notes"][0][0].as_int() == 0);
        ASSERT(ppqn_song["tracks"][0]["notes"][0][2].as_int() == 2);
    }

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
        imported, aur::PreviewSynth::Fm, 8000, 10);
    const aur::StereoPcm psg = aur::synthesize_preview(
        imported, aur::PreviewSynth::RetroPsg, 8000, 10);
    const aur::StereoPcm gm = aur::synthesize_preview(
        imported, aur::PreviewSynth::Gm, 8000, 10);
    ASSERT(fm.sample_rate == 8000);
    ASSERT(fm.samples.size() == psg.samples.size());
    ASSERT(gm.samples.size() == psg.samples.size());
    ASSERT(!fm.samples.empty());
    ASSERT(fm.samples != psg.samples);
    ASSERT(gm.samples != fm.samples);
    ASSERT(gm.samples != psg.samples);
    aur::Json alternateProgram = imported;
    alternateProgram["tracks"][0]["program"] = 0;
    const aur::StereoPcm alternateFm = aur::synthesize_preview(
        alternateProgram, aur::PreviewSynth::Fm, 8000, 10);
    ASSERT(fm.samples != alternateFm.samples);
    const aur::StereoPcm alternateGm = aur::synthesize_preview(
        alternateProgram, aur::PreviewSynth::Gm, 8000, 10);
    ASSERT(gm.samples != alternateGm.samples);
    bool bad_file_rejected = false;
    try {
        (void)aur::import_standard_midi({0, 1, 2});
    } catch (const std::runtime_error&) {
        bad_file_rejected = true;
    }
    ASSERT(bad_file_rejected);
}
