#include "mini_test.hpp"

#include "bestiary/RecordFields.hpp"

#include <string>

// The record fields: a new record has every field, an edit keeps the
// fields the editor does not show, and the numbers are clamped.
void test_record_fields()
{
    using namespace aur;

    // The fields of each kind are data.
    ASSERT(records::fieldsFor("actors").size() > 5);
    ASSERT(records::fieldsFor("classes").size() == 2);
    ASSERT(records::fieldsFor("races").size() == 11);
    ASSERT(records::fieldsFor("items").size() == 4);
    ASSERT(records::fieldsFor("weapons").size() == 8);
    ASSERT(records::fieldsFor("skills").size() == 5);
    ASSERT(records::fieldsFor("nonsense").empty());

    const Json race = records::newRecord("races", "elf", "Sylvan Elf");
    ASSERT(race["name"].as_string() == "Sylvan Elf");
    const Json race_edited = records::applyFields(
        "races", race, {{"habitat", "Canopy cities"}, {"lifespan", "Centuries"}});
    ASSERT(race_edited["habitat"].as_string() == "Canopy cities");
    ASSERT(race_edited["lifespan"].as_string() == "Centuries");

    // A new record: the fields the editor shows, and their defaults.
    const Json actor = records::newRecord("actors", "bram", "Bram");
    ASSERT(actor["name"].as_string() == "Bram");
    ASSERT(actor["hp"].is_number());
    ASSERT(actor["growth"].is_object());
    ASSERT(actor["growth"]["hp"].is_number());

    // An edit writes the fields it shows.
    const Json edited = records::applyFields(
        "actors", actor, {{"name", "Bram the Bold"}, {"hp", "80"}, {"level", "4"},
                           {"growth.hp", "12"}, {"growth.spd", "0.5"}});
    ASSERT(edited["name"].as_string() == "Bram the Bold");
    ASSERT(edited["hp"].as_int() == 80);
    ASSERT(edited["level"].as_int() == 4);
    ASSERT(edited["growth"]["hp"].as_int() == 12);
    ASSERT(edited["growth"]["spd"].as_int() == 0);

    // The fields the editor does not show are carried through.
    Json with_extra = actor;
    with_extra["note"] = "a note the editor does not show";
    const Json kept = records::applyFields("actors", with_extra, {{"hp", "10"}});
    ASSERT(kept["note"].as_string() == "a note the editor does not show");
    ASSERT(kept["hp"].as_int() == 10);

    // The numbers are clamped to their range.
    const Json clamped = records::applyFields("actors", actor, {{"level", "500"}});
    ASSERT(clamped["level"].as_int() == 99);
    const Json low = records::applyFields("actors", actor, {{"hp", "-4"}});
    ASSERT(low["hp"].as_int() == 1);

    // A weapon: the fraction and the pair.
    const Json weapon = records::newRecord("weapons", "axe", "Hand Axe");
    ASSERT(weapon["cd"].is_number());
    ASSERT(weapon["range"].is_array());
    const Json swung = records::applyFields(
        "weapons", weapon, {{"cd", "0,5"}, {"range.near", "1"}, {"range.far", "2"}});
    ASSERT(swung["cd"].as_number() > 0.4 && swung["cd"].as_number() < 0.6);
    ASSERT(swung["range"].as_array().size() == 2);
    ASSERT(swung["range"].as_array()[1].as_int() == 2);

    // The text of a field, for the form.
    ASSERT(records::fieldText(actor, records::fieldsFor("actors")[0]) == "Bram");
    const records::Field &hp_field = records::fieldsFor("actors")[5];
    ASSERT(records::fieldText(actor, hp_field) == std::to_string(actor["hp"].as_int()));
}
