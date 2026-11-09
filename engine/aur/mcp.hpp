#pragma once

#include "project.hpp"
#include <iosfwd>

namespace aur {

void run_mcp_server(Project& project, std::istream& input, std::ostream& output);

} // namespace aur
