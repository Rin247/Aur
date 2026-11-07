#include "mini_test.hpp"
#include "aur/audio/SongDocument.hpp"

namespace
{
    using aur::Json;
    using aur::JsonArray;
    using aur::JsonObject;
    using aur::audio::SongDocument;

    Json lane_track()
    {
        return aur::parse_json(
            "{\"name\":\"Lead\",\"channel\":0,"
            "\"notes\":[[0,60,4,100]],"
            "\"automation\":[{\"cc\":74,\"points\":[{\"step\":0,\"value\":40},"
            "{\"step\":8,\"value\":100}]}]}");
    }

    void test_lane_access()
    {
        Json track = lane_track();
        const Json *lane = SongDocument::lane_at(track, 74);
        ASSERT(lane != nullptr);
        ASSERT(SongDocument::lane_at(track, 11) == nullptr);
        ASSERT(SongDocument::lane_at_mut(track, 11) == nullptr);

        const auto points = SongDocument::lane_points(track, 74);
        ASSERT(points.size() == 2);
        ASSERT(points[0].step == 0 && points[0].value == 40);
        ASSERT(points[1].step == 8 && points[1].value == 100);

        // Points come back step-sorted even when stored out of order.
        Json *mut = SongDocument::lane_at_mut(track, 74);
        ASSERT(mut != nullptr);
        JsonArray reordered = JsonArray{
            JsonObject{{"step", 16}, {"value", 10}},
            JsonObject{{"step", 2}, {"value", 20}}};
        (*mut)["points"] = Json(std::move(reordered));
        const auto sorted = SongDocument::lane_points(track, 74);
        ASSERT(sorted.size() == 2);
        ASSERT(sorted[0].step == 2 && sorted[1].step == 16);

        ASSERT(SongDocument::lane_value_at(track, 74, 4, -1) == 20);
        ASSERT(SongDocument::lane_value_at(track, 74, 8, -1) == 20);
        ASSERT(SongDocument::lane_value_at(track, 74, 0, -1) == -1);
        ASSERT(SongDocument::lane_value_at(track, 74, 99, -1) == 10);
        ASSERT(SongDocument::lane_value_at(track, 11, 0, -1) == -1);
    }

    void test_lane_edit()
    {
        Json track = lane_track();

        SongDocument::set_lane_point(track, 74, 0, 90);
        auto points = SongDocument::lane_points(track, 74);
        ASSERT(points.size() == 2);
        ASSERT(points[0].step == 0 && points[0].value == 90);

        SongDocument::set_lane_point(track, 74, 4, 55);
        points = SongDocument::lane_points(track, 74);
        ASSERT(points.size() == 3);
        ASSERT(points[1].step == 4 && points[1].value == 55);

        SongDocument::set_lane_point(track, 74, 12, 200);
        points = SongDocument::lane_points(track, 74);
        ASSERT(points.back().value == 127);
        SongDocument::set_lane_point(track, 74, 12, -5);
        points = SongDocument::lane_points(track, 74);
        ASSERT(points.back().value == 0);

        SongDocument::clear_lane_point(track, 74, 4);
        points = SongDocument::lane_points(track, 74);
        ASSERT(points.size() == 3);
        ASSERT(points[1].step == 8);
        SongDocument::clear_lane_point(track, 74, 99);

        // Creating a lane on a track without automation.
        Json bare = aur::parse_json("{\"name\":\"Bass\",\"notes\":[]}");
        SongDocument::set_lane_point(bare, SongDocument::kLaneBend, 0, 1024);
        ASSERT(bare.contains("automation") && bare["automation"].is_array());
        ASSERT(SongDocument::lane_points(bare, SongDocument::kLaneBend).size() == 1);
        // Bend values are stored as-is (−8192..8191 contract) — only CC lanes
        // clamp to 0..127, enforced by the callers per lane type.
        const Json *bend = SongDocument::lane_at(bare, SongDocument::kLaneBend);
        ASSERT(bend != nullptr && (*bend)["cc"].as_int() == SongDocument::kLaneBend);

        // create_lane is idempotent.
        Json *lane_a = SongDocument::create_lane(bare, 7);
        Json *lane_b = SongDocument::create_lane(bare, 7);
        ASSERT(lane_a == lane_b);
        ASSERT(static_cast<int>(bare["automation"].as_array().size()) == 2);
    }
} // namespace

void test_automation_lanes()
{
    test_lane_access();
    test_lane_edit();
}
