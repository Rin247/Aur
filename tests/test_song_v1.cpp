#include "mini_test.hpp"
#include "aur/json.hpp"

#include <cstdio>
#include <string>

namespace {

// v1 song contract (decisions.md 2026-11-06): these keys must stay loadable.
// Values mirror projects/sylvi/music/overworld.json.
const char* kOverworld = R"({
 "bars": 8,
 "beats": 4,
 "gs": true,
 "loop_bar": 0,
 "name": "Aurelia",
 "reverb": 4,
 "tempo": 112,
 "tracks": [
  {
   "bank": 0, "channel": 0, "chorus": 0, "drum": false,
   "name": "French Horn", "pan": 64, "program": 60, "reverb": 0, "volume": 100,
   "notes": [[0, 74, 4, 92], [4, 81, 4, 92], [8, 78, 3, 92]]
  },
  {
   "bank": 0, "channel": 9, "chorus": 0, "drum": true,
   "name": "Percussion", "pan": 64, "program": 0, "reverb": 0, "volume": 90,
   "notes": [[0, 36, 1, 100]]
  }
 ]
})";

const char* kInn = R"({
 "bars": 16, "beats": 3, "gs": false, "loop_bar": 8,
 "name": "Inn", "preview_synth": "retro_psg", "tempo": 96,
 "tracks": [
  {"bank": 0, "channel": 1, "chorus": 0, "drum": false, "name": "Melody",
   "pan": 48, "program": 80, "reverb": 2, "volume": 88,
   "notes": [[0, 72, 6, 88]]}
 ]
})";

} // namespace

// Phase 0 safety net for P3: pins every v1 song-level and track-level key so
// the v2 migration can never silently drop one. Runs against inline fixtures
// mirroring real project files (overworld.json, inn.json).
void test_song_v1_contract() {
    const aur::Json overworld = aur::parse_json(kOverworld);

    // Song-level keys.
    ASSERT(overworld["bars"].as_int() == 8);
    ASSERT(overworld["beats"].as_int() == 4);
    ASSERT(overworld["gs"].as_bool());
    ASSERT(overworld["loop_bar"].as_int() == 0);
    ASSERT(overworld["name"].as_string() == "Aurelia");
    ASSERT(overworld["reverb"].as_int() == 4);
    ASSERT(overworld["tempo"].as_int() == 112);
    ASSERT(overworld.contains("preview_synth") == false); // optional in v1

    // Track-level keys.
    const aur::Json& horn = overworld["tracks"][0];
    ASSERT(horn["bank"].as_int() == 0);
    ASSERT(horn["channel"].as_int() == 0);
    ASSERT(horn["chorus"].as_int() == 0);
    ASSERT(horn["drum"].as_bool() == false);
    ASSERT(horn["name"].as_string() == "French Horn");
    ASSERT(horn["pan"].as_int() == 64);
    ASSERT(horn["program"].as_int() == 60);
    ASSERT(horn["reverb"].as_int() == 0);
    ASSERT(horn["volume"].as_int() == 100);
    ASSERT(horn["notes"].as_array().size() == 3);
    ASSERT(horn["notes"][0].as_array().size() == 4);
    ASSERT(horn["notes"][0][0].as_int() == 0);
    ASSERT(horn["notes"][0][1].as_int() == 74);
    ASSERT(horn["notes"][0][2].as_int() == 4);
    ASSERT(horn["notes"][0][3].as_int() == 92);

    // Alternate flavour: gs=false, preview_synth, different beats, loop_bar.
    const aur::Json inn = aur::parse_json(kInn);
    ASSERT(inn["gs"].as_bool() == false);
    ASSERT(inn["preview_synth"].as_string() == "retro_psg");
    ASSERT(inn["beats"].as_int() == 3);
    ASSERT(inn["loop_bar"].as_int() == 8);
    ASSERT(inn["tracks"][0]["pan"].as_int() == 48);
    ASSERT(inn["tracks"][0]["reverb"].as_int() == 2);

    // Scenes are optional in v1 (initialised on demand by the UI).
    ASSERT(overworld.contains("scenes") == false);
    ASSERT(inn.contains("scenes") == false);

    std::printf("  v1 contract pinned (overworld + inn flavours)\n");
}
