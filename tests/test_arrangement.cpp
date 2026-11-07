#include "mini_test.hpp"
#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"

namespace
{

    aur::Json base_song()
    {
        return aur::parse_json(
            "{\"name\":\"Arr\",\"tempo\":120,\"bars\":1,\"beats\":4,"
            "\"tracks\":[{\"name\":\"Lead\",\"channel\":0,\"program\":40,"
            "\"notes\":[]},{\"name\":\"Bass\",\"channel\":1,\"program\":33,"
            "\"notes\":[]}],"
            "\"scenes\":[{\"name\":\"A\",\"clips\":[{\"notes\":[[0,60,4,100],[12,67,4,100]]},"
            "{\"notes\":[[0,36,8,90]]}]},"
            "{\"name\":\"B\",\"clips\":[{\"notes\":[[0,72,4,110]]},null]}]}");
    }

} // namespace

// P6.5: arrangement placements + flatten.
void test_arrangement()
{
    using aur::audio::SongDocument;
    aur::Json song = base_song();

    // v1 / absent key reads as empty.
    ASSERT(SongDocument::placements(song).empty());
    ASSERT(SongDocument::placement_at(song, 0, 0) == nullptr);
    ASSERT(SongDocument::placement_at_mut(song, 0, 0) == nullptr);
    SongDocument::remove_placement(song, 0, 0); // no-op, no crash

    // Add placements; they sort by step.
    SongDocument::add_placement(song, SongDocument::Placement{0, 0, 0, 16});
    SongDocument::add_placement(song, SongDocument::Placement{1, 0, 16, 16});
    SongDocument::add_placement(song, SongDocument::Placement{0, 1, 0, 16});
    const std::vector<SongDocument::Placement> placed = SongDocument::placements(song);
    ASSERT(placed.size() == 3);
    ASSERT(placed[0].step == 0 && placed[1].step == 0 && placed[2].step == 16);
    ASSERT(placed[0].track == 0 && placed[1].track == 1 && placed[2].scene == 1);

    // Lookup by (step, track).
    const aur::Json *hit = SongDocument::placement_at(song, 16, 0);
    ASSERT(hit != nullptr && (*hit)["scene"].as_int() == 1);
    ASSERT(SongDocument::placement_at(song, 16, 1) == nullptr);

    // Negative / out-of-range values clamp (fresh song so the flatten tests
    // below see only the three deliberate placements).
    aur::Json clampSong = base_song();
    SongDocument::add_placement(clampSong, SongDocument::Placement{-1, -2, -3, 0});
    const std::vector<SongDocument::Placement> clamped =
        SongDocument::placements(clampSong);
    ASSERT(clamped.size() == 1);
    ASSERT(clamped[0].scene == 0 && clamped[0].track == 0 &&
           clamped[0].step == 0 && clamped[0].length == 1);
    ASSERT(SongDocument::placements(song).size() == 3);

    // Flatten: clip notes offset to absolute steps, clipped to the window.
    aur::Json flat = SongDocument::flatten_arrangement(song);
    ASSERT(flat["tracks"].as_array().size() == 2);
    const aur::JsonArray &lead = flat["tracks"].as_array()[0]["notes"].as_array();
    // Track 0: scene A at 0 (notes 0,12) + scene B at 16 (note 0 → abs 16).
    ASSERT(lead.size() == 3);
    ASSERT(lead[0].as_array()[0].as_int() == 0 && lead[0].as_array()[1].as_int() == 60);
    ASSERT(lead[1].as_array()[0].as_int() == 12);
    ASSERT(lead[2].as_array()[0].as_int() == 16 && lead[2].as_array()[1].as_int() == 72);
    // Track 1: scene A bass only; scene B has a null clip → nothing.
    const aur::JsonArray &bass = flat["tracks"].as_array()[1]["notes"].as_array();
    ASSERT(bass.size() == 1 && bass[0].as_array()[0].as_int() == 0);

    // Length window clips notes that run past the placement end.
    aur::Json song2 = base_song();
    SongDocument::add_placement(song2, SongDocument::Placement{0, 1, 0, 4});
    const aur::Json flat2 = SongDocument::flatten_arrangement(song2);
    const aur::JsonArray &bass2 = flat2["tracks"].as_array()[1]["notes"].as_array();
    ASSERT(bass2.size() == 1 && bass2[0].as_array()[0].as_int() == 0 &&
           bass2[0].as_array()[2].as_int() == 4);

    // Note beyond the placement window is dropped.
    aur::Json song3 = base_song();
    SongDocument::add_placement(song3, SongDocument::Placement{0, 0, 16, 8});
    const aur::Json flat3 = SongDocument::flatten_arrangement(song3);
    const aur::JsonArray &lead3 = flat3["tracks"].as_array()[0]["notes"].as_array();
    // Clip A notes at 0 and 12 → abs 16 and 28; 28 >= 16+8 → dropped,
    // note at abs 16 clipped to the 8-step window... wait: clip note lengths
    // are 4, so both fit: note 0→abs16 (len 4), note 12→abs28 >= 24 → dropped.
    ASSERT(lead3.size() == 1);
    ASSERT(lead3[0].as_array()[0].as_int() == 16 && lead3[0].as_array()[2].as_int() == 4);

    // bars computed from the flattened content.
    ASSERT(flat3["bars"].as_int() == 2);

    // Remove by (step, track).
    SongDocument::remove_placement(song, 16, 0);
    ASSERT(SongDocument::placements(song).size() == 2);
    ASSERT(SongDocument::placement_at(song, 16, 0) == nullptr);
}
