#include "mini_test.hpp"

#include "bestiary/BestiaryData.hpp"

#include "aur/project.hpp"

#include <string>
#include <vector>

// The creature record, without a widget: a new creature has a full set
// of numbers, an edit keeps the fields the editor does not show, and the
// id that is taken is not handed out twice.
void test_bestiary_record()
{
    using namespace aur;

    // The fields the editor shows.
    ASSERT(bestiary::isEditorField("name"));
    ASSERT(bestiary::isEditorField("hp"));
    ASSERT(!bestiary::isEditorField("aggro"));

    // A new creature: the defaults.
    const Json fresh = bestiary::newRecord("slime", "Green Slime");
    ASSERT(fresh["name"].as_string() == "Green Slime");
    ASSERT(fresh["hp"].is_number());
    ASSERT(fresh["hp"].as_int() > 0);
    ASSERT(fresh["ai"].is_string());

    // An id that is free.
    JsonObject enemies;
    enemies["slime"] = fresh;
    ASSERT(bestiary::suggestId(enemies, "Green Slime") == "green_slime");
    ASSERT(bestiary::suggestId(enemies, "Slime") == "slime_2");

    // An edit keeps the fields the editor does not show.
    Json record = fresh;
    record["aggro"] = 150;
    record["drops"] = Json(JsonArray{Json(JsonObject{{"item", "candy"}, {"p", 0.15}})});
    const Json edited = bestiary::applyFields(record, {{"name", "Blue Slime"}, {"hp", "24"},
                                                       {"atk", "6"}, {"def", "-2"}});
    ASSERT(edited["name"].as_string() == "Blue Slime");
    ASSERT(edited["hp"].as_int() == 24);
    ASSERT(edited["atk"].as_int() == 6);
    // The numbers are clamped to their range.
    ASSERT(edited["def"].as_int() == 0);
    // The fields the editor does not show are still there.
    ASSERT(edited["aggro"].as_int() == 150);
    ASSERT(edited["drops"].is_array());
    ASSERT(edited["drops"].as_array().size() == 1);

    // The artwork: the creature's own graphic, or nothing when the
    // project does not have one.
    JsonObject graphics;
    graphics["slime_g"] = Json(JsonObject{
        {"w", 4},
        {"h", 4},
        {"palette", JsonArray{"#000000", "#ffffff"}},
        {"frames", JsonObject{{"0", JsonArray{".1..", ".1..", "....", "...."}}}}});
    Project project;
    project.graphics = graphics;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
    ASSERT(bestiary::artwork(project, "slime_g", width, height, pixels));
    ASSERT(width == 4);
    ASSERT(height == 4);
    ASSERT(pixels.size() == 4 * 4 * 4);
    ASSERT(!bestiary::artwork(project, "missing", width, height, pixels));

    // The label is the name, or the id when there is none.
    ASSERT(bestiary::label("slime", fresh) == "Green Slime");
    Json nameless = Json(JsonObject{});
    ASSERT(bestiary::label("slime", nameless) == "slime");
}
