#pragma once

#include "json.hpp"
#include <string>
#include <map>
#include <vector>

namespace aur {

constexpr int N = 1;
constexpr int E = 2;
constexpr int S = 4;
constexpr int W = 8;

std::map<int, std::string> tile_groups(const Json& ts);
int mask_at(const Json& m, const Json& ts, const std::string& layer, int x, int y, const std::string& group, const std::map<int, std::string>& groups = {});
void retile(Json& m, const Json& ts, const std::string& layer, int x, int y, const std::map<int, std::string>& groups = {});
void paint(Json& m, const Json& ts, const std::string& layer, int x, int y, const std::string& group);
void erase(Json& m, const Json& ts, const std::string& layer, int x, int y, int fill = -1);
void retile_all(Json& m, const Json& ts, const std::string& layer);

} // namespace aur
