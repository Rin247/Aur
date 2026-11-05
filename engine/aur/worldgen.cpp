#include "project.hpp"
#include "world_topology.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aur {
namespace {

constexpr double pi = 3.14159265358979323846;

struct Vec3 {
    double x;
    double y;
    double z;
};

double smooth(double value) {
    return value * value * (3.0 - 2.0 * value);
}

double mix(double a, double b, double amount) {
    return a + (b - a) * amount;
}

uint32_t hash_lattice(int x, int y, int z, uint32_t seed) {
    uint32_t value = seed ^ (static_cast<uint32_t>(x) * 0x8da6b343u);
    value ^= static_cast<uint32_t>(y) * 0xd8163841u;
    value ^= static_cast<uint32_t>(z) * 0xcb1ab31fu;
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

double lattice_value(int x, int y, int z, uint32_t seed) {
    return static_cast<double>(hash_lattice(x, y, z, seed)) /
               static_cast<double>(UINT32_MAX) * 2.0 - 1.0;
}

double value_noise(const Vec3& point, uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(point.x));
    const int y0 = static_cast<int>(std::floor(point.y));
    const int z0 = static_cast<int>(std::floor(point.z));
    const double fx = smooth(point.x - x0);
    const double fy = smooth(point.y - y0);
    const double fz = smooth(point.z - z0);

    const double z_low = mix(
        mix(lattice_value(x0, y0, z0, seed), lattice_value(x0 + 1, y0, z0, seed), fx),
        mix(lattice_value(x0, y0 + 1, z0, seed),
            lattice_value(x0 + 1, y0 + 1, z0, seed), fx), fy);
    const double z_high = mix(
        mix(lattice_value(x0, y0, z0 + 1, seed),
            lattice_value(x0 + 1, y0, z0 + 1, seed), fx),
        mix(lattice_value(x0, y0 + 1, z0 + 1, seed),
            lattice_value(x0 + 1, y0 + 1, z0 + 1, seed), fx), fy);
    return mix(z_low, z_high, fz);
}

double fractal_noise(const Vec3& point, uint32_t seed, double frequency,
                     int octaves, double persistence = 0.52) {
    double total = 0.0;
    double amplitude = 1.0;
    double amplitude_sum = 0.0;
    for (int octave = 0; octave < octaves; ++octave) {
        const double scale = frequency * (1 << octave);
        total += value_noise({point.x * scale, point.y * scale, point.z * scale},
                             seed + static_cast<uint32_t>(octave) * 0x9e3779b9u) * amplitude;
        amplitude_sum += amplitude;
        amplitude *= persistence;
    }
    return total / amplitude_sum;
}

std::vector<int> generate_hydrology(
    int width, int height, int sea_level, uint32_t seed,
    const std::vector<std::vector<int>>& elevation,
    const std::vector<std::vector<int>>& moisture,
    const std::vector<std::vector<bool>>& land) {
    const int cell_count = width * height;
    std::vector<int> water(static_cast<size_t>(cell_count), 0);
    const auto index_at = [width](int x, int y) { return y * width + x; };
    std::vector<int> visit_stamp(static_cast<size_t>(cell_count), 0);
    int basin_stamp = 0;
    constexpr std::array<std::pair<int, int>, 8> neighbors{{
        {-1, -1}, {0, -1}, {1, -1}, {-1, 0},
        {1, 0}, {-1, 1}, {0, 1}, {1, 1}
    }};

    int lake_count = 0;
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!land[static_cast<size_t>(y)][static_cast<size_t>(x)] ||
                elevation[static_cast<size_t>(y)][static_cast<size_t>(x)] <= sea_level + 35 ||
                hash_lattice(x, y, 0x4c414b45, seed) % 4u != 0u) continue;

            const int start = index_at(x, y);
            if (water[static_cast<size_t>(start)] != 0) continue;
            int spill_level = std::numeric_limits<int>::max();
            bool enclosed = true;
            for (const auto& [dx, dy] : neighbors) {
                const int nx = wrapX(x + dx, width);
                const int ny = y + dy;
                if (!land[static_cast<size_t>(ny)][static_cast<size_t>(nx)]) {
                    enclosed = false;
                    break;
                }
                spill_level = std::min(
                    spill_level, elevation[static_cast<size_t>(ny)][static_cast<size_t>(nx)]);
            }
            const int basin_floor = elevation[static_cast<size_t>(y)][static_cast<size_t>(x)];
            if (!enclosed || spill_level - basin_floor < 18) continue;

            std::vector<int> basin;
            std::vector<int> pending{start};
            ++basin_stamp;
            visit_stamp[static_cast<size_t>(start)] = basin_stamp;
            while (!pending.empty() && basin.size() <= 96) {
                const int current = pending.back();
                pending.pop_back();
                basin.push_back(current);
                const int cx = current % width;
                const int cy = current / width;
                for (const auto& [dx, dy] : neighbors) {
                    const int nx = wrapX(cx + dx, width);
                    const int ny = cy + dy;
                    if (ny < 0 || ny >= height) continue;
                    const int next = index_at(nx, ny);
                    if (visit_stamp[static_cast<size_t>(next)] == basin_stamp ||
                        !land[static_cast<size_t>(ny)][static_cast<size_t>(nx)] ||
                        elevation[static_cast<size_t>(ny)][static_cast<size_t>(nx)] >= spill_level) {
                        continue;
                    }
                    visit_stamp[static_cast<size_t>(next)] = basin_stamp;
                    pending.push_back(next);
                }
            }
            if (basin.size() < 3 || basin.size() > 96) continue;
            for (const int cell : basin) water[static_cast<size_t>(cell)] = 2;
            ++lake_count;
        }
    }

    const int desired_lakes = std::max(1, cell_count / 2200);
    if (lake_count < desired_lakes) {
        std::vector<int> lowland_candidates;
        for (int y = 2; y < height - 2; ++y) {
            for (int x = 0; x < width; ++x) {
                const int center = elevation[static_cast<size_t>(y)][static_cast<size_t>(x)];
                if (!land[static_cast<size_t>(y)][static_cast<size_t>(x)] ||
                    center < sea_level + 25 || center > sea_level + 180) continue;
                int neighborhood_total = 0;
                int neighborhood_count = 0;
                bool inland = true;
                for (int dy = -2; dy <= 2; ++dy) {
                    for (int dx = -2; dx <= 2; ++dx) {
                        const int nx = wrapX(x + dx, width);
                        const int ny = y + dy;
                        if (!land[static_cast<size_t>(ny)][static_cast<size_t>(nx)]) {
                            inland = false;
                            continue;
                        }
                        if (std::abs(dx) == 2 || std::abs(dy) == 2) {
                            neighborhood_total +=
                                elevation[static_cast<size_t>(ny)][static_cast<size_t>(nx)];
                            ++neighborhood_count;
                        }
                    }
                }
                if (inland && neighborhood_count > 0 &&
                    center + 4 < neighborhood_total / neighborhood_count) {
                    lowland_candidates.push_back(index_at(x, y));
                }
            }
        }
        std::sort(lowland_candidates.begin(), lowland_candidates.end(),
                  [&elevation, &width, seed](int a, int b) {
            const int ax = a % width;
            const int ay = a / width;
            const int bx = b % width;
            const int by = b / width;
            const int ah = elevation[static_cast<size_t>(ay)][static_cast<size_t>(ax)];
            const int bh = elevation[static_cast<size_t>(by)][static_cast<size_t>(bx)];
            if (ah != bh) return ah < bh;
            return hash_lattice(ax, ay, 0x4c414b45, seed) <
                   hash_lattice(bx, by, 0x4c414b45, seed);
        });

        std::vector<int> lake_centers;
        int created_lakes = lake_count;
        for (const int center : lowland_candidates) {
            if (created_lakes >= desired_lakes) break;
            const int x = center % width;
            const int y = center / width;
            const bool too_close = std::any_of(
                lake_centers.begin(), lake_centers.end(), [x, y, width](int other) {
                    const int dx = std::abs(other % width - x);
                    return dx < 8 && std::abs(other / width - y) < 6;
                });
            if (too_close) continue;
            const uint32_t lake_hash = hash_lattice(x, y, 0x6c616b65, seed);
            const int radius_x = 1 + static_cast<int>((lake_hash >> 4) % 3u);
            const int radius_y = 1 + static_cast<int>((lake_hash >> 9) % 2u);
            for (int dy = -radius_y; dy <= radius_y; ++dy) {
                for (int dx = -radius_x; dx <= radius_x; ++dx) {
                    const int nx = wrapX(x + dx, width);
                    const int ny = y + dy;
                    if (dx * dx * radius_y * radius_y +
                            dy * dy * radius_x * radius_x >
                        radius_x * radius_x * radius_y * radius_y) continue;
                    if (land[static_cast<size_t>(ny)][static_cast<size_t>(nx)] &&
                        elevation[static_cast<size_t>(ny)][static_cast<size_t>(nx)] <=
                            sea_level + 220) {
                        const size_t lake_cell = static_cast<size_t>(index_at(nx, ny));
                        water[lake_cell] = 2;
                    }
                }
            }
            lake_centers.push_back(center);
            ++created_lakes;
        }
    }

    std::vector<int> order;
    order.reserve(static_cast<size_t>(cell_count));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (land[static_cast<size_t>(y)][static_cast<size_t>(x)]) {
                order.push_back(index_at(x, y));
            }
        }
    }
    std::sort(order.begin(), order.end(), [&elevation, width](int a, int b) {
        const int ax = a % width;
        const int ay = a / width;
        const int bx = b % width;
        const int by = b / width;
        return elevation[static_cast<size_t>(ay)][static_cast<size_t>(ax)] >
               elevation[static_cast<size_t>(by)][static_cast<size_t>(bx)];
    });

    std::vector<int> receiver(static_cast<size_t>(cell_count), -1);
    for (const int cell : order) {
        if (water[static_cast<size_t>(cell)] == 2) continue;
        const int x = cell % width;
        const int y = cell / width;
        const int current_height = elevation[static_cast<size_t>(y)][static_cast<size_t>(x)];
        int lowest_height = current_height;
        int lowest_cell = -1;
        for (const auto& [dx, dy] : neighbors) {
            const int nx = wrapX(x + dx, width);
            const int ny = y + dy;
            if (ny < 0 || ny >= height) continue;
            const int next_height = land[static_cast<size_t>(ny)][static_cast<size_t>(nx)]
                ? elevation[static_cast<size_t>(ny)][static_cast<size_t>(nx)] : sea_level;
            if (next_height < lowest_height) {
                lowest_height = next_height;
                lowest_cell = land[static_cast<size_t>(ny)][static_cast<size_t>(nx)]
                    ? index_at(nx, ny) : -2;
            }
        }
        receiver[static_cast<size_t>(cell)] = lowest_cell;
    }

    std::vector<double> accumulation(static_cast<size_t>(cell_count), 0.0);
    for (const int cell : order) {
        const int x = cell % width;
        const int y = cell / width;
        accumulation[static_cast<size_t>(cell)] =
            1.0 + moisture[static_cast<size_t>(y)][static_cast<size_t>(x)] / 50.0;
    }
    for (const int cell : order) {
        const int downstream = receiver[static_cast<size_t>(cell)];
        if (downstream >= 0) {
            accumulation[static_cast<size_t>(downstream)] +=
                accumulation[static_cast<size_t>(cell)];
        }
    }

    const double river_threshold = std::max(12.0, cell_count / 900.0);
    for (const int cell : order) {
        if (water[static_cast<size_t>(cell)] != 0 ||
            accumulation[static_cast<size_t>(cell)] < river_threshold) continue;
        water[static_cast<size_t>(cell)] = 1;
    }
    return water;
}

} // namespace

Json generate_world_map(const std::string& map_id, const std::string& name,
                        int width, int height, int seed, const std::string& topology,
                        int water_level, int vegetation, int decay, int axial_tilt) {
    constexpr std::uint64_t maximum_world_cells = 2560ull * 1280;
    const std::uint64_t world_cells = width > 0 && height > 0
        ? static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) : 0;
    if (map_id.empty() || width < 24 || width > 2560 || height < 16 || height > 1280 ||
        world_cells > maximum_world_cells ||
        water_level < 0 || water_level > 100 || vegetation < 0 || vegetation > 100 ||
        decay < 0 || decay > 100 || axial_tilt < 0 || axial_tilt > 359 ||
        (topology != "continents" && topology != "archipelago" &&
         topology != "supercontinent" && topology != "fractured")) {
        throw std::runtime_error(
            "World dimensions must be at least 24x16 and fit within 2560x1280 "
            "(3,276,800 terrain cells); topology and generation controls must also be valid.");
    }

    double frequency = 2.2;
    int octaves = 5;
    double persistence = 0.52;
    if (topology == "archipelago") {
        frequency = 4.2;
        persistence = 0.62;
    } else if (topology == "supercontinent") {
        frequency = 1.35;
        octaves = 4;
        persistence = 0.48;
    } else if (topology == "fractured") {
        frequency = 2.8;
        persistence = 0.68;
    }

    const uint32_t base_seed = static_cast<uint32_t>(seed);
    const int sea_level = 500 + (water_level - 50) * 3;
    const double sea_threshold = (sea_level - 500) / 300.0;
    Json map = new_map(map_id, width, height, "world", "world", "action", "hd2d", 7);
    JsonObject& map_layers = map["layers"].as_object();
    map_layers.erase("ground");
    map_layers.erase("objects");
    map_layers.erase("above");
    map["name"] = name;
    map["seed"] = seed;
    map["worldgen"] = JsonObject{
        {"seed", seed}, {"topology", topology}, {"style", "hd2d"},
        {"water_level", water_level}, {"sea_level", sea_level},
        {"vegetation", vegetation}, {"decay", decay}, {"axial_tilt", axial_tilt}
    };

    std::vector<std::vector<bool>> land(
        static_cast<size_t>(height), std::vector<bool>(static_cast<size_t>(width), false));
    std::vector<std::vector<int>> elevation(
        static_cast<size_t>(height), std::vector<int>(static_cast<size_t>(width), 0));
    std::vector<std::vector<int>> moisture(
        static_cast<size_t>(height), std::vector<int>(static_cast<size_t>(width), 0));
    std::vector<std::vector<int>> temperature(
        static_cast<size_t>(height), std::vector<int>(static_cast<size_t>(width), -1));

    for (int y = 0; y < height; ++y) {
        const double latitude = pi * (0.5 - static_cast<double>(y) /
            std::max(1, height - 1));
        const double cos_latitude = std::cos(latitude);
        for (int x = 0; x < width; ++x) {
            const double longitude = 2.0 * pi * x / width;
            const Vec3 point{
                cos_latitude * std::cos(longitude),
                std::sin(latitude),
                cos_latitude * std::sin(longitude)
            };
            const double warp_x = fractal_noise(point, base_seed ^ 0xa511e9b3u, 2.1, 2);
            const double warp_y = fractal_noise(point, base_seed ^ 0x63d83595u, 2.1, 2);
            const Vec3 warped{
                point.x + warp_x * 0.16,
                point.y + warp_y * 0.16,
                point.z - (warp_x + warp_y) * 0.08
            };
            const double continental = fractal_noise(warped, base_seed, frequency,
                                                      octaves, persistence);
            double ridge = 0.0;
            if (topology == "fractured") {
                ridge = (1.0 - std::abs(fractal_noise(
                    warped, base_seed ^ 0xc2b2ae35u, frequency * 2.7, 3, 0.58))) * 0.12;
            }
            const double surface = continental + ridge;
            const int raw_elevation = std::clamp(
                static_cast<int>(std::lround(500.0 + surface * 300.0)), 0, 1000);
            const bool is_land = surface > sea_threshold;
            land[static_cast<size_t>(y)][static_cast<size_t>(x)] = is_land;
            elevation[static_cast<size_t>(y)][static_cast<size_t>(x)] = raw_elevation;

            const double climate_latitude = std::clamp(
                std::abs(2.0 * static_cast<double>(y) / std::max(1, height - 1) - 1.0),
                0.0, 1.0);
            temperature[static_cast<size_t>(y)][static_cast<size_t>(x)] =
                is_land ? static_cast<int>(std::lround((1.0 - climate_latitude) * 100.0)) : -1;

            const double humidity = fractal_noise(
                point, base_seed ^ 0x27d4eb2fu, 3.4, 4, 0.57);
            const double coast_boost = std::clamp(
                (raw_elevation - sea_level) / 160.0, 0.0, 1.0);
            moisture[static_cast<size_t>(y)][static_cast<size_t>(x)] = is_land
                ? std::clamp(static_cast<int>(std::lround(
                    50.0 + humidity * 43.0 + coast_boost * 12.0)), 0, 100) : 0;
        }
    }

    const std::vector<int> hydrology = generate_hydrology(
        width, height, sea_level, base_seed, elevation, moisture, land);
    JsonArray biome_rows;
    JsonArray temperature_rows;
    JsonArray elevation_rows;
    JsonArray moisture_rows;
    JsonArray water_rows;
    JsonArray override_rows;
    for (int y = 0; y < height; ++y) {
        JsonArray biome_row;
        JsonArray temperature_row;
        JsonArray elevation_row;
        JsonArray moisture_row;
        JsonArray water_row;
        JsonArray override_row;
        for (int x = 0; x < width; ++x) {
            const size_t row = static_cast<size_t>(y);
            const size_t column = static_cast<size_t>(x);
            const bool is_land = land[row][column];
            const int current_elevation = elevation[row][column];
            const int current_temperature = temperature[row][column];
            const int current_moisture = moisture[row][column];
            elevation_row.emplace_back(current_elevation);
            moisture_row.emplace_back(current_moisture);
            temperature_row.emplace_back(current_temperature);
            water_row.emplace_back(hydrology[row * static_cast<size_t>(width) + column]);
            override_row.emplace_back(-1);
            if (!is_land) {
                biome_row.emplace_back("");
                continue;
            }

            bool coast = false;
            for (const auto& [dx, dy] :
                 std::array<std::pair<int, int>, 4>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}}) {
                const int xx = wrapX(x + dx, width);
                const int yy = y + dy;
                if (yy >= 0 && yy < height &&
                    !land[static_cast<size_t>(yy)][static_cast<size_t>(xx)]) coast = true;
            }

            std::string biome_id = "verdant_plains";
            const uint32_t variation = hash_lattice(x, y, 0, base_seed) % 100;
            if (coast) {
                biome_id = "coast_and_islands";
            } else if (current_elevation > sea_level + 205) {
                biome_id = current_temperature < 25
                    ? "frostfall_tundra" : "alpine_highlands";
            } else if (current_temperature < 18) {
                biome_id = "frostfall_tundra";
            } else if (current_moisture < 27 && current_temperature > 48) {
                biome_id = "desert_wastes";
            } else if (decay > 0 && variation < static_cast<uint32_t>(decay / 6)) {
                biome_id = "ancient_ruins";
            } else if (current_moisture > 58 &&
                       variation < static_cast<uint32_t>(18 + vegetation * 62 / 100)) {
                biome_id = "old_growth_forest";
            } else if (current_moisture > 76 && vegetation > 35) {
                biome_id = "tropical_jungle";
            } else if (current_elevation > sea_level + 95) {
                biome_id = "alpine_highlands";
            }
            biome_row.emplace_back(biome_id);
        }
        biome_rows.emplace_back(std::move(biome_row));
        temperature_rows.emplace_back(std::move(temperature_row));
        elevation_rows.emplace_back(std::move(elevation_row));
        moisture_rows.emplace_back(std::move(moisture_row));
        water_rows.emplace_back(std::move(water_row));
        override_rows.emplace_back(std::move(override_row));
    }
    map["layers"]["biomes"] = std::move(biome_rows);
    map["layers"]["temperature"] = std::move(temperature_rows);
    map["layers"]["elevation"] = std::move(elevation_rows);
    map["layers"]["moisture"] = std::move(moisture_rows);
    map["layers"]["water"] = std::move(water_rows);
    map["layers"]["terrain_overrides"] = std::move(override_rows);

    map["entities"] = JsonArray{};
    return map;
}

} // namespace aur
