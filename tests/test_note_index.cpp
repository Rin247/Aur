#include "mini_test.hpp"
#include "music/NoteIndex.hpp"

#include "aur/audio/SongDocument.hpp"

#include <set>

#include <vector>

namespace
{
    using aur::Json;
    using aur::JsonArray;
    using aur::JsonObject;
    using aur::editor::NoteIndex;

    // A note is [step, pitch, length, velocity, {props}]; the id lives in props.
    Json note(int id, int step = 0)
    {
        Json n = JsonArray{step, 60, 4, 90};
        aur::audio::SongDocument::set_note_prop(n, "id", id);
        return n;
    }

    Json track_with(const std::vector<Json> &notes)
    {
        JsonArray arr;
        for (const Json &n : notes)
            arr.push_back(n);
        return JsonObject{{"name", "Lead"}, {"notes", arr}};
    }

    void test_ids_maps_in_order()
    {
        Json track = track_with({note(7, 0), note(3, 4), note(9, 8)});
        const auto ids = NoteIndex::ids(track);
        ASSERT(ids.size() == 3);
        ASSERT(ids[0] == 7);
        ASSERT(ids[1] == 3);
        ASSERT(ids[2] == 9);
    }

    void test_unidentified_note_maps_to_minus_one()
    {
        // A v1 note without a props object has no id; the mapping records -1 so
        // a selection covering it cannot pretend to be stable.
        Json n = JsonArray{0, 60, 4, 90};
        Json track = track_with({note(5, 0), n});
        const auto ids = NoteIndex::ids(track);
        ASSERT(ids.size() == 2);
        ASSERT(ids[0] == 5);
        ASSERT(ids[1] == -1);
    }

    void test_empty_and_malformed_tracks_are_safe()
    {
        ASSERT(NoteIndex::ids(Json{}).empty());
        ASSERT(NoteIndex::ids(JsonObject{{"name", "Empty"}}).empty());
        Json bad = JsonObject{{"notes", JsonObject{}}};
        ASSERT(NoteIndex::ids(bad).empty());
    }

    void test_to_ids_and_back_survives_reordering()
    {
        // Selection {0, 2} -> ids {7, 9}; after a re-sort the same ids resolve
        // to {0, 1} — the selection followed the notes, not the slots.
        Json track = track_with({note(7, 0), note(3, 4), note(9, 8)});
        const auto indexToId = NoteIndex::ids(track);
        const std::set<int> selected{0, 2};
        const auto ids = NoteIndex::toIds(selected, indexToId);
        ASSERT(ids == std::set<int>({7, 9}));

        JsonArray reordered;
        reordered.push_back(track["notes"][2]); // id 9
        reordered.push_back(track["notes"][1]); // id 3
        reordered.push_back(track["notes"][0]); // id 7
        Json shuffled = track_with({});
        shuffled["notes"] = reordered;

        const auto newIndexToId = NoteIndex::ids(shuffled);
        const auto indices = NoteIndex::toIndices(ids, newIndexToId);
        ASSERT(indices == std::set<int>({0, 2}));
    }

    void test_deleted_ids_are_dropped()
    {
        Json track = track_with({note(7, 0), note(3, 4), note(9, 8)});
        const auto before = NoteIndex::ids(track);
        const auto ids = NoteIndex::toIds(std::set<int>{0, 1, 2}, before);
        ASSERT(ids.size() == 3);

        // Deleting the id-3 note leaves {7, 9}; the mapping shrinks to two
        // entries and toIndices must not resurrect anything.
        JsonArray shortened;
        shortened.push_back(track["notes"][0]);
        shortened.push_back(track["notes"][2]);
        Json afterDelete = track_with({});
        afterDelete["notes"] = shortened;

        const auto after = NoteIndex::ids(afterDelete);
        ASSERT(after.size() == 2);
        const auto indices = NoteIndex::toIndices(ids, after);
        ASSERT(indices == std::set<int>({0, 1}));
    }

    void test_out_of_range_indices_are_ignored()
    {
        const std::vector<int> mapping{7, -1, 9};
        ASSERT(NoteIndex::toIds(std::set<int>{-1, 0, 1, 2, 5}, mapping) ==
               std::set<int>({7, 9}));
        ASSERT(NoteIndex::toIndices(std::set<int>{7, 9}, mapping) ==
               std::set<int>({0, 2}));
    }
} // namespace

void test_note_index()
{
    test_ids_maps_in_order();
    test_unidentified_note_maps_to_minus_one();
    test_empty_and_malformed_tracks_are_safe();
    test_to_ids_and_back_survives_reordering();
    test_deleted_ids_are_dropped();
    test_out_of_range_indices_are_ignored();
}
