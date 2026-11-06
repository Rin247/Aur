#include "aur/audio/midi.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace aur {
namespace {

constexpr int midi_ticks_per_quarter = 480;
constexpr int ticks_per_step = midi_ticks_per_quarter / 4;

int midi_tick_to_step(std::uint32_t tick, std::uint16_t division) {
    const std::uint64_t scaled_tick = static_cast<std::uint64_t>(tick) * 4;
    const std::uint64_t rounded_step = (scaled_tick + division / 2) / division;
    return static_cast<int>(std::min<std::uint64_t>(
        rounded_step, static_cast<std::uint64_t>(std::numeric_limits<int>::max())));
}

struct MidiEvent {
    uint32_t tick = 0;
    std::vector<uint8_t> bytes;
    int order = 0;
};

struct ChannelState {
    int program = 0;
    int bank_msb = 0;
    int bank_lsb = 0;
    int volume = 100;
    int pan = 64;
    bool initialized = false;
    JsonArray notes;
    std::map<int, std::vector<std::pair<uint32_t, int>>> active_notes;
};

uint16_t read_u16_be(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 2 > data.size()) throw std::runtime_error("MIDI file is truncated.");
    return static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1]);
}

uint32_t read_u32_be(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + 4 > data.size()) throw std::runtime_error("MIDI file is truncated.");
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

void append_u16_be(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(static_cast<uint8_t>(value >> 8));
    data.push_back(static_cast<uint8_t>(value & 0xff));
}

void append_u32_be(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back(static_cast<uint8_t>(value >> 24));
    data.push_back(static_cast<uint8_t>((value >> 16) & 0xff));
    data.push_back(static_cast<uint8_t>((value >> 8) & 0xff));
    data.push_back(static_cast<uint8_t>(value & 0xff));
}

void append_vlq(std::vector<uint8_t>& data, uint32_t value) {
    std::array<uint8_t, 4> buffer{};
    size_t count = 0;
    buffer[count++] = static_cast<uint8_t>(value & 0x7f);
    while ((value >>= 7) != 0 && count < buffer.size()) {
        buffer[count++] = static_cast<uint8_t>((value & 0x7f) | 0x80);
    }
    while (count > 0) data.push_back(buffer[--count]);
}

uint32_t read_vlq(const std::vector<uint8_t>& data, size_t& cursor, size_t end) {
    uint32_t value = 0;
    for (int index = 0; index < 4; ++index) {
        if (cursor >= end) throw std::runtime_error("MIDI variable-length quantity is truncated.");
        const uint8_t byte = data[cursor++];
        value = (value << 7) | (byte & 0x7f);
        if ((byte & 0x80) == 0) return value;
    }
    throw std::runtime_error("MIDI variable-length quantity exceeds four bytes.");
}

void append_chunk(std::vector<uint8_t>& file, const char* id,
                  const std::vector<uint8_t>& payload) {
    file.insert(file.end(), id, id + 4);
    append_u32_be(file, static_cast<uint32_t>(payload.size()));
    file.insert(file.end(), payload.begin(), payload.end());
}

std::string track_name(const Json& track, size_t index) {
    return track.contains("name") && track["name"].is_string()
        ? track["name"].as_string() : "Track " + std::to_string(index + 1);
}

void add_meta_event(std::vector<MidiEvent>& events, uint32_t tick, uint8_t type,
                    const std::vector<uint8_t>& data, int order) {
    std::vector<uint8_t> bytes{0xff, type};
    append_vlq(bytes, static_cast<uint32_t>(data.size()));
    bytes.insert(bytes.end(), data.begin(), data.end());
    events.push_back({tick, std::move(bytes), order});
}

std::vector<uint8_t> serialize_track(std::vector<MidiEvent> events) {
    std::stable_sort(events.begin(), events.end(), [](const MidiEvent& left, const MidiEvent& right) {
        if (left.tick != right.tick) return left.tick < right.tick;
        return left.order < right.order;
    });
    std::vector<uint8_t> track;
    uint32_t previous_tick = 0;
    for (const MidiEvent& event : events) {
        append_vlq(track, event.tick - previous_tick);
        track.insert(track.end(), event.bytes.begin(), event.bytes.end());
        previous_tick = event.tick;
    }
    append_vlq(track, 0);
    track.insert(track.end(), {0xff, 0x2f, 0x00});
    return track;
}

void parse_track(const std::vector<uint8_t>& data, size_t begin, size_t end,
                 std::uint16_t division, JsonArray& tracks, Json& song,
                 int& tempo_microseconds,
                 int& time_signature_numerator, int& time_signature_denominator,
                 bool& found_tempo, bool is_first_track) {
    std::map<int, ChannelState> channels;
    size_t cursor = begin;
    uint32_t tick = 0;
    uint8_t running_status = 0;
    while (cursor < end) {
        tick += read_vlq(data, cursor, end);
        if (cursor >= end) throw std::runtime_error("MIDI event is truncated.");
        uint8_t status = data[cursor++];
        if (status < 0x80) {
            if (running_status < 0x80 || running_status >= 0xf0) {
                throw std::runtime_error("MIDI event uses running status without a channel status.");
            }
            --cursor;
            status = running_status;
        } else if (status < 0xf0) {
            running_status = status;
        } else {
            running_status = 0;
        }

        if (status == 0xff) {
            if (cursor >= end) throw std::runtime_error("MIDI meta event is truncated.");
            const uint8_t type = data[cursor++];
            const uint32_t length = read_vlq(data, cursor, end);
            if (length > end - cursor) throw std::runtime_error("MIDI meta event payload is truncated.");
            if (type == 0x03 && is_first_track) {
                song["name"] = std::string(data.begin() + static_cast<std::ptrdiff_t>(cursor),
                                           data.begin() + static_cast<std::ptrdiff_t>(cursor + length));
            } else if (type == 0x51 && length == 3 && !found_tempo) {
                tempo_microseconds = (static_cast<int>(data[cursor]) << 16) |
                    (static_cast<int>(data[cursor + 1]) << 8) | data[cursor + 2];
                found_tempo = tempo_microseconds > 0;
            } else if (type == 0x58 && length >= 2) {
                time_signature_numerator = std::max(1, static_cast<int>(data[cursor]));
                const int exponent = std::min(5, static_cast<int>(data[cursor + 1]));
                time_signature_denominator = 1 << exponent;
            }
            cursor += length;
            if (type == 0x2f) break;
            continue;
        }
        if (status == 0xf0 || status == 0xf7) {
            const uint32_t length = read_vlq(data, cursor, end);
            if (length > end - cursor) throw std::runtime_error("MIDI SysEx payload is truncated.");
            JsonArray message{static_cast<int>(status)};
            for (uint32_t byte = 0; byte < length; ++byte) {
                message.emplace_back(static_cast<int>(data[cursor + byte]));
            }
            song["sysex"].as_array().emplace_back(std::move(message));
            cursor += length;
            continue;
        }
        if (status >= 0xf0) throw std::runtime_error("Unsupported MIDI system event.");

        const int command = status & 0xf0;
        const int channel = status & 0x0f;
        const int data_count = command == 0xc0 || command == 0xd0 ? 1 : 2;
        if (cursor + static_cast<size_t>(data_count) > end) {
            throw std::runtime_error("MIDI channel event is truncated.");
        }
        const int first = data[cursor++];
        const int second = data_count == 2 ? data[cursor++] : 0;
        if (first >= 0x80 || second >= 0x80) {
            throw std::runtime_error("MIDI channel data byte is outside the 7-bit range.");
        }
        ChannelState& state = channels[channel];
        state.initialized = true;
        if (command == 0x90 && second > 0) {
            state.active_notes[first].emplace_back(tick, second);
        } else if (command == 0x80 || (command == 0x90 && second == 0)) {
            auto active = state.active_notes.find(first);
            if (active == state.active_notes.end() || active->second.empty()) continue;
            const auto [start, velocity] = active->second.front();
            active->second.erase(active->second.begin());
            const int step = midi_tick_to_step(start, division);
            const int end_step = midi_tick_to_step(tick, division);
            state.notes.emplace_back(JsonArray{
                step, first, std::max(1, end_step - step), velocity
            });
        } else if (command == 0xc0) {
            state.program = first;
        } else if (command == 0xb0) {
            if (first == 0) state.bank_msb = second;
            else if (first == 32) state.bank_lsb = second;
            else if (first == 7) state.volume = second;
            else if (first == 10) state.pan = second;
        }
    }

    for (auto& [channel, state] : channels) {
        for (auto& [pitch, pending] : state.active_notes) {
            for (const auto& [start, velocity] : pending) {
                const int start_step = midi_tick_to_step(start, division);
                const int end_step = midi_tick_to_step(tick, division);
                state.notes.emplace_back(JsonArray{
                    start_step, pitch, std::max(1, end_step - start_step), velocity
                });
            }
        }
        JsonArray sorted_notes = std::move(state.notes);
        std::stable_sort(sorted_notes.begin(), sorted_notes.end(), [](const Json& left, const Json& right) {
            return left[0].as_int() < right[0].as_int();
        });
        tracks.emplace_back(JsonObject{
            {"name", "Channel " + std::to_string(channel + 1)},
            {"channel", channel}, {"program", state.program},
            {"bank", state.bank_msb * 128 + state.bank_lsb},
            {"bank_msb", state.bank_msb}, {"bank_lsb", state.bank_lsb},
            {"volume", state.volume}, {"pan", state.pan},
            {"drum", channel == 9}, {"notes", std::move(sorted_notes)}
        });
    }
}

} // namespace

Json import_standard_midi(const std::vector<uint8_t>& data, const std::string& fallback_name) {
    if (data.size() < 14 || std::string(data.begin(), data.begin() + 4) != "MThd") {
        throw std::runtime_error("Not a Standard MIDI File.");
    }
    const uint32_t header_length = read_u32_be(data, 4);
    if (header_length < 6 || 8u + header_length > data.size()) {
        throw std::runtime_error("MIDI header length is invalid.");
    }
    const uint16_t format = read_u16_be(data, 8);
    const uint16_t declared_tracks = read_u16_be(data, 10);
    const uint16_t division = read_u16_be(data, 12);
    if (format > 1 || declared_tracks == 0) {
        throw std::runtime_error("Only MIDI Type 0 and Type 1 files are supported.");
    }
    if ((division & 0x8000) != 0 || division == 0) {
        throw std::runtime_error("SMPTE-timed MIDI files are not supported.");
    }

    Json song = JsonObject{
        {"name", fallback_name}, {"tempo", 120}, {"bars", 1},
        {"beats", 4}, {"ticks_per_quarter", static_cast<int>(division)},
        {"tracks", JsonArray{}}, {"sysex", JsonArray{}}
    };
    int tempo_microseconds = 500000;
    int time_signature_numerator = 4;
    int time_signature_denominator = 4;
    bool found_tempo = false;
    size_t cursor = 8 + header_length;
    for (uint16_t index = 0; index < declared_tracks; ++index) {
        if (cursor + 8 > data.size() ||
            std::string(data.begin() + static_cast<std::ptrdiff_t>(cursor),
                        data.begin() + static_cast<std::ptrdiff_t>(cursor + 4)) != "MTrk") {
            throw std::runtime_error("MIDI track chunk is missing or truncated.");
        }
        const uint32_t length = read_u32_be(data, cursor + 4);
        cursor += 8;
        if (length > data.size() - cursor) throw std::runtime_error("MIDI track data is truncated.");
        const size_t track_end = cursor + length;
        parse_track(data, cursor, track_end, division, song["tracks"].as_array(), song,
                    tempo_microseconds, time_signature_numerator,
                    time_signature_denominator, found_tempo, index == 0);
        cursor = track_end;
    }
    song["tempo"] = std::clamp(
        static_cast<int>(std::lround(60000000.0 / tempo_microseconds)), 30, 240);
    song["beats"] = time_signature_numerator;
    song["time_signature"] = JsonArray{
        time_signature_numerator, time_signature_denominator
    };

    int last_step = 0;
    for (const Json& track : song["tracks"].as_array()) {
        if (!track.contains("notes") || !track["notes"].is_array()) continue;
        for (const Json& note : track["notes"].as_array()) {
            if (note.is_array() && note.as_array().size() >= 3) {
                last_step = std::max(last_step, note[0].as_int() + note[2].as_int());
            }
        }
    }
    song["bars"] = std::clamp((last_step + 15) / 16, 1, 128);
    return song;
}

std::vector<uint8_t> export_standard_midi(const Json& song) {
    if (!song.contains("tracks") || !song["tracks"].is_array()) {
        throw std::runtime_error("Song has no MIDI tracks to export.");
    }
    const int tempo = song.contains("tempo") && song["tempo"].is_number()
        ? std::clamp(song["tempo"].as_int(), 30, 240) : 120;
    const int beats = song.contains("beats") && song["beats"].is_number()
        ? std::clamp(song["beats"].as_int(), 1, 32) : 4;
    const int denominator = song.contains("time_signature") &&
        song["time_signature"].is_array() && song["time_signature"].as_array().size() >= 2
        ? std::clamp(song["time_signature"][1].as_int(), 1, 32) : 4;
    int denominator_exponent = 0;
    while ((1 << denominator_exponent) < denominator && denominator_exponent < 5) {
        ++denominator_exponent;
    }

    std::vector<MidiEvent> conductor_events;
    const std::string name = song.contains("name") && song["name"].is_string()
        ? song["name"].as_string() : "Aur Song";
    add_meta_event(conductor_events, 0, 0x03,
                   std::vector<uint8_t>(name.begin(), name.end()), 0);
    const uint32_t micros_per_quarter = static_cast<uint32_t>(60000000 / tempo);
    add_meta_event(conductor_events, 0, 0x51, {
        static_cast<uint8_t>((micros_per_quarter >> 16) & 0xff),
        static_cast<uint8_t>((micros_per_quarter >> 8) & 0xff),
        static_cast<uint8_t>(micros_per_quarter & 0xff)
    }, 1);
    add_meta_event(conductor_events, 0, 0x58, {
        static_cast<uint8_t>(beats), static_cast<uint8_t>(denominator_exponent), 24, 8
    }, 2);

    if (song.contains("sysex") && song["sysex"].is_array()) {
        int order = 3;
        for (const Json& message : song["sysex"].as_array()) {
            if (!message.is_array() || message.as_array().empty()) continue;
            const int status = message[0].as_int();
            if (status != 0xf0 && status != 0xf7) continue;
            std::vector<uint8_t> sysex;
            for (size_t index = 1; index < message.as_array().size(); ++index) {
                const int byte = message[index].as_int();
                if (byte < 0 || byte > 255) {
                    throw std::runtime_error("MIDI SysEx data byte is outside the byte range.");
                }
                sysex.push_back(static_cast<uint8_t>(byte));
            }
            std::vector<uint8_t> bytes{static_cast<uint8_t>(status)};
            append_vlq(bytes, static_cast<uint32_t>(sysex.size()));
            bytes.insert(bytes.end(), sysex.begin(), sysex.end());
            conductor_events.push_back({0, std::move(bytes), order++});
        }
    }

    std::vector<std::vector<uint8_t>> chunks;
    chunks.push_back(serialize_track(std::move(conductor_events)));
    const JsonArray& tracks = song["tracks"].as_array();
    for (size_t track_index = 0; track_index < tracks.size(); ++track_index) {
        const Json& track = tracks[track_index];
        const int channel = track.contains("channel") && track["channel"].is_number()
            ? std::clamp(track["channel"].as_int(), 0, 15)
            : static_cast<int>(track_index % 16);
        const int program = track.contains("program") && track["program"].is_number()
            ? std::clamp(track["program"].as_int(), 0, 127) : 0;
        const int bank = track.contains("bank") && track["bank"].is_number()
            ? std::clamp(track["bank"].as_int(), 0, 16383) : 0;
        const int volume = track.contains("volume") && track["volume"].is_number()
            ? std::clamp(track["volume"].as_int(), 0, 127) : 100;
        const int pan = track.contains("pan") && track["pan"].is_number()
            ? std::clamp(track["pan"].as_int(), 0, 127) : 64;
        std::vector<MidiEvent> events;
        const std::string track_label = track_name(track, track_index);
        add_meta_event(events, 0, 0x03,
                       std::vector<uint8_t>(track_label.begin(), track_label.end()), 0);
        events.push_back({0, {static_cast<uint8_t>(0xb0 | channel), 0,
                              static_cast<uint8_t>((bank >> 7) & 0x7f)}, 1});
        events.push_back({0, {static_cast<uint8_t>(0xb0 | channel), 32,
                              static_cast<uint8_t>(bank & 0x7f)}, 2});
        events.push_back({0, {static_cast<uint8_t>(0xc0 | channel),
                              static_cast<uint8_t>(program)}, 3});
        events.push_back({0, {static_cast<uint8_t>(0xb0 | channel), 7,
                              static_cast<uint8_t>(volume)}, 4});
        events.push_back({0, {static_cast<uint8_t>(0xb0 | channel), 10,
                              static_cast<uint8_t>(pan)}, 5});

        if (track.contains("notes") && track["notes"].is_array()) {
            int order = 10;
            for (const Json& note : track["notes"].as_array()) {
                if (!note.is_array() || note.as_array().size() < 3) continue;
                const int step = std::max(note[0].as_int(), 0);
                const int pitch = std::clamp(note[1].as_int(), 0, 127);
                const int length = std::clamp(note[2].as_int(), 1, 65535 / ticks_per_step);
                const int velocity = note.as_array().size() >= 4
                    ? std::clamp(note[3].as_int(), 1, 127) : 90;
                const uint32_t start = static_cast<uint32_t>(step * ticks_per_step);
                const uint32_t end = static_cast<uint32_t>((step + length) * ticks_per_step);
                events.push_back({start,
                    {static_cast<uint8_t>(0x90 | channel), static_cast<uint8_t>(pitch),
                     static_cast<uint8_t>(velocity)}, 100 + order++});
                events.push_back({end,
                    {static_cast<uint8_t>(0x80 | channel), static_cast<uint8_t>(pitch), 0}, 10 + order++});
            }
        }
        chunks.push_back(serialize_track(std::move(events)));
    }

    std::vector<uint8_t> file{'M', 'T', 'h', 'd'};
    append_u32_be(file, 6);
    append_u16_be(file, 1);
    append_u16_be(file, static_cast<uint16_t>(chunks.size()));
    append_u16_be(file, midi_ticks_per_quarter);
    for (const std::vector<uint8_t>& chunk : chunks) append_chunk(file, "MTrk", chunk);
    return file;
}

} // namespace aur
