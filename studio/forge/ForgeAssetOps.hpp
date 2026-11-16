#pragma once

#include <string>
#include "aur/json.hpp"

namespace aur::forge {

std::string asset_id(const std::string& value);
std::string unique_id(const JsonObject& records, std::string base);
void apply_role_preset(Json& spec, const std::string& preset_name);

} // namespace aur::forge
