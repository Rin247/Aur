#pragma once

#include "json.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace aur {

Json import_standard_midi(const std::vector<std::uint8_t>& data,
                          const std::string& fallback_name = "Imported MIDI");
std::vector<std::uint8_t> export_standard_midi(const Json& song);

} // namespace aur
