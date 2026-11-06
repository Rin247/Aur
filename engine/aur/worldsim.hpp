#pragma once
#include "json.hpp"
#include "world_topology.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aur::worldsim {

enum class Field {
    Temperature = 0,
    Moisture,
    WindU,
    WindV,
    Cloud,
    Rain,
    Snowpack,
    Count
};

inline constexpr int kFieldCount = static_cast<int>(Field::Count);

struct WorldSimParams {
    int width = 32;
    int height = 16;
    std::uint64_t seed = 1;
    double seconds_per_tick = 600.0;
};

class WorldSim {
public:
    WorldSim() = default;
    explicit WorldSim(const WorldSimParams& params);

    void advance(int ticks);
    void pin(Field field, int x, int y, float value);
    void clear_pin(Field field, int x, int y);
    bool is_pinned(Field field, int x, int y) const;
    float value(Field field, int x, int y) const;
    std::uint64_t tick_count() const noexcept { return tick_count_; }
    const WorldSimParams& params() const noexcept { return params_; }

    std::uint64_t hash() const;
    Json to_json() const;
    static WorldSim from_json(const Json& d);

private:
    WorldSimParams params_;
    std::uint64_t tick_count_ = 0;
    std::vector<std::vector<float>> fields_{};
    std::vector<std::vector<bool>> pins_{};
};

} // namespace aur::worldsim
