#include "forge/ForgeAssetOps.hpp"
#include <cctype>

namespace aur::forge {

std::string asset_id(const std::string& value) {
    std::string id;
    bool separator = false;
    for (unsigned char ch : value) {
        if (std::isalnum(ch)) {
            if (separator && !id.empty()) id.push_back('_');
            id.push_back(static_cast<char>(std::tolower(ch)));
            separator = false;
        } else {
            separator = true;
        }
    }
    return id;
}

std::string unique_id(const JsonObject& records, std::string base) {
    if (base.empty()) base = "asset";
    std::string id = base;
    for (int suffix = 2; records.contains(id); ++suffix) id = base + "_" + std::to_string(suffix);
    return id;
}

void apply_role_preset(Json& spec, const std::string& preset_name) {
    Json& colors = spec["colors"];
    Json& parts = spec["parts"];
    if (preset_name == "Mage") {
        colors["top"] = "#7957a0";
        colors["accent"] = "#84bde0";
        parts["role"] = "mage";
    } else if (preset_name == "Knight") {
        colors["top"] = "#858a9a";
        colors["bottom"] = "#4b5368";
        colors["accent"] = "#4876a5";
        parts["role"] = "guard";
    } else if (preset_name == "Villager") {
        colors["top"] = "#b9a16d";
        colors["bottom"] = "#74523b";
        parts["role"] = "villager";
    } else {
        parts["role"] = "adventurer";
    }
}

} // namespace aur::forge
