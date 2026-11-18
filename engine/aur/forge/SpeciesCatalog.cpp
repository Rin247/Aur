#include "aur/forge.hpp"
#include "aur/project.hpp"

#include <algorithm>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace aur {
namespace {

std::string color_or(const Json& colors, const std::string& key, const std::string& fallback) {
    return colors.is_object() && colors.contains(key) && colors[key].is_string()
        ? colors[key].as_string() : fallback;
}

std::string shade(const std::string& color, double amount) {
    const Rgb source = hex_to_rgba(color);
    const auto channel = [amount](int value) {
        return std::clamp(static_cast<int>(value * amount), 0, 255);
    };
    return rgb_to_hex({channel(source.r), channel(source.g), channel(source.b), 255});
}

std::string lighten(const std::string& color, double amount) {
    const Rgb source = hex_to_rgba(color);
    const auto channel = [amount](int value) {
        return std::clamp(static_cast<int>(value + (255 - value) * amount), 0, 255);
    };
    return rgb_to_hex({channel(source.r), channel(source.g), channel(source.b), 255});
}

} // namespace

Json build_palette(const Json& colors) {
    const std::string skin = color_or(colors, "skin", "#e8b58a");
    const std::string hair = color_or(colors, "hair", "#563b36");
    const std::string top = color_or(colors, "top", "#3f7652");
    const std::string bottom = color_or(colors, "bottom", "#594454");
    const std::vector<std::string> palette = snap_palette({
        "#000000", color_or(colors, "outline", "#30283a"),
        skin, shade(skin, 0.72), hair, shade(hair, 0.68),
        top, shade(top, 0.70), bottom, shade(bottom, 0.70),
        color_or(colors, "trim", "#e4c477"), color_or(colors, "metal", "#c5d5d8"),
        color_or(colors, "eye", "#342c38"), lighten(hair, 0.25),
        color_or(colors, "accent", "#c66b56"), lighten(skin, 0.25)
    });
    JsonArray result;
    for (const std::string& color : palette) result.emplace_back(color);
    return Json(result);
}

std::vector<RaceInfo> character_race_catalog() {
    return {
        {"sylvani",            "Sylvani",              RaceGroup::Mortal,       false},
        {"solani",             "Solani",               RaceGroup::Mortal,       false},
        {"gharuk",             "Gharuk",               RaceGroup::Mortal,       false},
        {"kaeli",              "Kaeli (Human)",        RaceGroup::Mortal,       false},
        {"featherfolk",        "Featherfolk",          RaceGroup::Mortal,       false},
        {"beastkin",           "Beastkin",             RaceGroup::Mortal,       false},
        {"spirits",            "Spirits",              RaceGroup::Supernatural, true},
        {"treesprites_dryads", "Treesprites / Dryads", RaceGroup::Supernatural, true},
        {"fairies_nymphs",     "Fairies / Nymphs",     RaceGroup::Supernatural, true},
        {"sirens_mermaids",    "Sirens / Mermaids",    RaceGroup::Supernatural, true},
        {"elementals",         "Elementals",           RaceGroup::Supernatural, true},
        {"undead",             "Undead",               RaceGroup::Supernatural, true},
        {"vampires",           "Vampires",             RaceGroup::Supernatural, false},
        {"veylkai",            "Dragons (Veyl'kai)",   RaceGroup::Supernatural, true},
        {"were_creatures",     "Were-Creatures",       RaceGroup::Supernatural, false}
    };
}

std::vector<std::pair<std::string, std::string>> character_race_options() {
    std::vector<std::pair<std::string, std::string>> out;
    for (const RaceInfo& info : character_race_catalog()) out.emplace_back(info.id, info.label);
    return out;
}

std::vector<std::pair<std::string, std::string>> character_variant_options(const std::string& race) {
    if (race == "beastkin" || race == "were_creatures") {
        return {{"wolf", "Wolf"}, {"cat", "Cat"}, {"dog", "Dog"}, {"tiger", "Tiger"},
                {"lion", "Lion"}, {"bear", "Bear"}};
    }
    if (race == "featherfolk") {
        return {{"raven", "Raven"}, {"owl", "Owl"}, {"hawk", "Hawk"}, {"parrot", "Parrot"},
                {"songbird", "Songbird"}, {"peacock", "Peacock"}};
    }
    if (race == "undead") {
        return {{"ghoul", "Ghoul"}, {"wraith", "Wraith"}, {"lich", "Lich"}};
    }
    if (race == "vampires") {
        return {{"human_derived", "Human-derived"}, {"sylvani_derived", "Sylvani-derived"}};
    }
    if (race == "veylkai") {
        return {{"mortal_guise", "Mortal guise"}, {"dragon_form", "Dragon form"}};
    }
    return {{"standard", "Standard"}};
}

std::vector<std::pair<std::string, std::string>> alignment_options() {
    return {
        {"unaligned",    "Unaligned"},
        {"lawful_good",  "Lawful Good"},
        {"neutral_good", "Neutral Good"},
        {"chaotic_good", "Chaotic Good"},
        {"lawful_evil",  "Lawful Evil"},
        {"neutral_evil", "Neutral Evil"},
        {"chaotic_evil", "Chaotic Evil"}
    };
}

std::vector<std::pair<std::string, std::string>> affinity_options() {
    return {
        {"random", "Auto (by seed)"},
        {"none",   "None"},
        {"ignis",  "Ignis"},
        {"terran", "Terran"},
        {"aqua",   "Aqua"},
        {"zephyr", "Zephyr"},
        {"aether", "Aether"},
        {"umbral", "Umbral"}
    };
}

std::vector<std::pair<std::string, std::string>> entity_role_options() {
    return {
        {"main_character", "Main Character"},
        {"npc",            "NPC"},
        {"follower",       "Follower / Companion"}
    };
}

std::vector<std::pair<std::string, std::string>> hair_style_options() {
    return {
        {"short", "Short"},
        {"long",  "Long"},
        {"curly", "Curly"}
    };
}

std::vector<std::pair<std::string, std::string>> accessory_options() {
    return {
        {"none",     "None"},
        {"circlet",  "Circlet"},
        {"headband", "Headband"},
        {"scarf",    "Scarf"}
    };
}

bool is_supported_character_race(const std::string& race) {
    static const auto races = character_race_options();
    return std::any_of(races.begin(), races.end(),
                   [&race](const auto& entry) { return entry.first == race; });
}

bool is_valid_alignment(const std::string& alignment) {
    static const std::vector<std::string> values = {
        "unaligned", "lawful_good", "lawful_evil", "neutral_good",
        "neutral_evil", "chaotic_good", "chaotic_evil"
    };
    return std::find(values.begin(), values.end(), alignment) != values.end();
}

bool is_valid_affinity(const std::string& affinity) {
    static const std::vector<std::string> values = {
        "random", "none", "ignis", "terran", "aqua", "zephyr", "aether", "umbral"
    };
    return std::find(values.begin(), values.end(), affinity) != values.end();
}

Json random_character_spec(int seed, const std::string& name, const std::string& age_group,
                       const std::string& gender, const std::string& presentation,
                       const std::string& race, const std::string& affinity,
                       const std::string& alignment, const std::string& role,
                       const std::string& variant) {
    std::mt19937 rng(static_cast<uint32_t>(seed));
    const auto pick = [&rng](const std::vector<std::string>& values) {
        return values[static_cast<size_t>(rng() % values.size())];
    };
    if (!is_supported_character_race(race)) {
        throw std::invalid_argument("Character race/category is not supported.");
    }
    if (!is_valid_affinity(affinity) || !is_valid_alignment(alignment) ||
        (role != "main_character" && role != "npc" && role != "follower")) {
        throw std::invalid_argument("Character affinity, alignment, or role is invalid.");
    }
    const auto variants = character_variant_options(race);
    std::string selected_variant = variant;
    if (variant == "random") {
        std::vector<std::string> values;
        for (const auto& option : variants) values.push_back(option.first);
        selected_variant = pick(values);
    } else if (std::none_of(variants.begin(), variants.end(),
                            [&variant](const auto& option) { return option.first == variant; })) {
        throw std::invalid_argument("Character appearance variant is invalid for the selected race.");
    }

    const std::vector<std::string> skin_tones = race == "sylvani"
        ? std::vector<std::string>{"#f2d0cb", "#e7b8ae", "#d9aa9c", "#c5a083"}
        : race == "solani"
            ? std::vector<std::string>{"#c5a080", "#a97a58", "#80583f", "#573a2d", "#321f1b"}
            : race == "gharuk"
                ? std::vector<std::string>{"#87915b", "#6f7d4a", "#9d8653", "#86533a", "#573a2d", "#321f1b"}
                : race == "kaeli"
                ? std::vector<std::string>{"#f6e5d1", "#e8c8a9", "#d9a47e", "#b9815f", "#8e5e45", "#684331", "#472d24", "#2d1b14"}
                : race == "featherfolk"
                    ? std::vector<std::string>{"#f6e5d1", "#e8c8a9", "#d9a47e", "#b9815f", "#8e5e45", "#684331"}
                    : race == "beastkin" || race == "were_creatures"
                        ? selected_variant == "wolf"
                            ? std::vector<std::string>{"#d9d4c8", "#89847c", "#3e3a3b", "#a99c8b"}
                            : selected_variant == "cat"
                                ? std::vector<std::string>{"#c58a54", "#d9cdb4", "#3e3a3b", "#89847c"}
                                : selected_variant == "dog"
                                    ? std::vector<std::string>{"#d2a35f", "#80583f", "#c58a54", "#3e3a3b"}
                                    : selected_variant == "tiger"
                                        ? std::vector<std::string>{"#d68a3a", "#c96c32", "#e0ae64"}
                                        : selected_variant == "lion"
                                            ? std::vector<std::string>{"#d2a35f", "#b47b50", "#e0c58c"}
                                            : std::vector<std::string>{"#57483c", "#80644b", "#c2a887", "#3e3a3b"}
                        : race == "undead"
                            ? std::vector<std::string>{"#babaa6", "#898e83", "#646b69", "#aaa195"}
                            : race == "spirits" || race == "elementals" ||
                              race == "treesprites_dryads" ||
                              race == "fairies_nymphs" || race == "sirens_mermaids" ||
                              race == "veylkai"
                                ? std::vector<std::string>{"#e9dfc5", "#b5d4cb", "#9eafba", "#8d9c8b"}
                                : std::vector<std::string>{"#f3d0b1", "#d9a47e", "#a96f51", "#754b3a"};
    const std::vector<std::string> hair_tones = race == "sylvani"
        ? std::vector<std::string>{"#d9d3c4", "#e1c66c", "#c2c1c8", "#9d563c", "#f0e8d8"}
        : race == "solani"
            ? std::vector<std::string>{"#241a1a", "#382a35", "#704737", "#9d563c", "#e1c66c"}
            : race == "gharuk"
                ? std::vector<std::string>{"#302822", "#463426", "#574232", "#272626"}
                : race == "beastkin" || race == "were_creatures"
                ? std::vector<std::string>{"#d9cdb4", "#89847c", "#3e3a3b", "#c58a54", "#b47b50", "#d2a35f"}
                : race == "featherfolk"
                    ? selected_variant == "raven" ? std::vector<std::string>{"#22252b", "#414853", "#a5adb5"}
                      : selected_variant == "owl" ? std::vector<std::string>{"#8a6547", "#d1b588", "#4b382d"}
                      : selected_variant == "hawk" ? std::vector<std::string>{"#9d563c", "#c4a16d", "#463b32"}
                      : selected_variant == "parrot" ? std::vector<std::string>{"#3c8b5a", "#c84c3a", "#e6c447"}
                      : selected_variant == "peacock" ? std::vector<std::string>{"#1e6d80", "#267f55", "#d6b649"}
                      : std::vector<std::string>{"#527ca4", "#c4b397", "#d6d5ca"}
                    : std::vector<std::string>{"#382a35", "#9d563c", "#e1c66c", "#6f8796", "#854f91"};
    std::string selected_affinity = affinity;
    const bool affinity_race = race == "spirits" || race == "treesprites_dryads" ||
                           race == "fairies_nymphs" ||
                           race == "sirens_mermaids" || race == "elementals" ||
                           race == "veylkai" || race == "undead";
    if (selected_affinity == "random") {
        selected_affinity = affinity_race
            ? pick({"ignis", "terran", "aqua", "zephyr", "aether", "umbral"})
            : "none";
    } else if (selected_affinity == "none" && affinity_race) {
        selected_affinity = pick({"ignis", "terran", "aqua", "zephyr", "aether", "umbral"});
    }
    const std::map<std::string, std::string> affinity_colors = {
        {"ignis", "#e85e3f"}, {"terran", "#75934a"}, {"aqua", "#398ec2"},
        {"zephyr", "#9fc9a2"}, {"aether", "#e6c45e"}, {"umbral", "#78529a"}
    };
    std::string accent = pick({"#c94d49", "#d9b84e", "#5e9d8a", "#8b74c8"});
    if (affinity_colors.contains(selected_affinity)) accent = affinity_colors.at(selected_affinity);
    if (race == "undead" && affinity_colors.contains(selected_affinity)) {
        accent = lighten(affinity_colors.at(selected_affinity), 0.25);
    }
    Json colors = JsonObject{
        {"skin", pick(skin_tones)},
        {"hair", pick(hair_tones)},
        {"eye", pick({"#2f694f", "#3d67a2", "#78523b"})},
        {"top", pick({"#3e8061", "#3b6ea5", "#a74640", "#7957a0", "#b58b3b"})},
        {"bottom", pick({"#5d4855", "#4b5c77", "#805b3d"})},
        {"accent", accent}
    };
    Json parts = JsonObject{
        {"age_group", age_group},
        {"hair_style", pick({"short", "long", "curly"})},
        {"presentation", presentation},
        {"race", race},
        {"variant", selected_variant},
        {"affinity", selected_affinity},
        {"alignment", alignment},
        {"entity_role", role},
        {"role", pick({"adventurer", "mage", "guard", "traveler", "villager"})},
        {"bottom", pick({"pants", "skirt", "shorts"})}
    };
    if (race == "sylvani" || race == "solani") parts["ears"] = "elf";
    return Json(JsonObject{{"name", name}, {"age_group", age_group}, {"race", race},
                       {"affinity", selected_affinity}, {"alignment", alignment},
                       {"entity_role", role},
                       {"gender", gender}, {"colors", colors}, {"parts", parts}});
}

void register_parts(const Json&) {}

} // namespace aur
