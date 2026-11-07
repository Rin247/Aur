#include "mini_test.hpp"
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace
{

    // Same v1 fixture as test_song_v1.cpp (overworld flavour).
    const char *kV1 = R"({
 "bars": 8, "beats": 4, "gs": true, "loop_bar": 2,
 "name": "Aurelia", "reverb": 4, "tempo": 112,
 "tracks": [
  {"bank": 0, "channel": 0, "chorus": 0, "drum": false, "name": "Horn",
   "pan": 64, "program": 60, "reverb": 0, "volume": 100,
   "notes": [[0, 74, 4, 92]]}
 ]
})";

    std::string write_temp(const char *name, const char *text)
    {
        const std::string path = std::string(name) + ".json";
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(text, static_cast<std::streamsize>(std::string(text).size()));
        return path;
    }

    std::string read_file(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

} // namespace

// P3.2: v1 → v2 migration keeps every v1 key readable, adds v2 keys, is
// idempotent, and save() writes a .bak from the previous file.
void test_song_v2_migration()
{
    // 1. Migration adds v2 keys without touching v1 keys.
    aur::Json song = aur::parse_json(kV1);
    ASSERT(aur::audio::SongDocument::load(song));
    ASSERT(song["format"].as_string() == "2");
    ASSERT(song["ticks_per_step"].as_int() == 120);
    ASSERT(song["tempo_map"].as_array().size() == 1);
    ASSERT(song["tempo_map"][0]["bpm"].as_int() == 112);
    ASSERT(song["time_sig_map"][0]["num"].as_int() == 4);
    ASSERT(song["loop"]["start_step"].as_int() == 32);
    ASSERT(song["loop"]["end_step"].as_int() == 128);
    // v1 keys still readable after migration.
    ASSERT(song["gs"].as_bool());
    ASSERT(song["reverb"].as_int() == 4);
    ASSERT(song["tempo"].as_int() == 112);
    ASSERT(song["tracks"][0]["program"].as_int() == 60);
    ASSERT(song["tracks"][0]["notes"][0][2].as_int() == 4);

    // 2. Migration is idempotent.
    const std::string once = aur::dump_json(song);
    aur::audio::SongDocument::migrate(song);
    ASSERT(aur::dump_json(song) == once);

    // 3. Map accessors with fallbacks.
    ASSERT(aur::audio::SongDocument::bpm_at(song, 0) == 112);
    ASSERT(aur::audio::SongDocument::bpm_at(song, 5000) == 112);
    ASSERT(aur::audio::SongDocument::beats_at(song, 999) == 4);
    aur::audio::SongDocument::set_tempo_at(song, 256, 96);
    ASSERT(aur::audio::SongDocument::bpm_at(song, 255) == 112);
    ASSERT(aur::audio::SongDocument::bpm_at(song, 256) == 96);
    aur::audio::SongDocument::set_time_sig_at(song, 256, 3, 4);
    ASSERT(aur::audio::SongDocument::beats_at(song, 256) == 3);

    // 4. Save writes the file and a .bak from the previous content.
    const std::string path = write_temp("daw_p3_v2_probe", kV1);
    const std::string first = read_file(path);
    aur::Json disk = aur::parse_json(first);
    ASSERT(aur::audio::SongDocument::load(disk));
    ASSERT(aur::audio::SongDocument::save(disk, path));
    ASSERT(read_file(path) != first);
    ASSERT(read_file(path + ".bak") == first);
    std::remove(path.c_str());
    std::remove((path + ".bak").c_str());

    // 5. load() rejects non-song JSON.
    aur::Json notSong = aur::parse_json("{\"foo\": 1}");
    ASSERT(aur::audio::SongDocument::load(notSong) == false);
}
