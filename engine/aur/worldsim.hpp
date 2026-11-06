#pragma once
#include "json.hpp"
#include "world_topology.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aur::worldsim
{

    enum class Field
    {
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

    struct WorldSimParams
    {
        int width = 32;
        int height = 16;
        std::uint64_t seed = 1;
        double seconds_per_tick = 600.0;
        double diffusion_rate = 0.06;
        double wind_step = 0.02;
        double cloud_growth = 0.03;
        double cloud_decay = 0.015;
        double rain_rate = 0.10;
        double snow_threshold = 0.15;
        double snow_melt = 0.05;
    };

    class WorldSim
    {
    public:
        WorldSim() = default;
        explicit WorldSim(const WorldSimParams &params);

        void set_heights(const std::vector<float> &heights);
        void advance(int ticks);
        void pin(Field field, int x, int y, float value);
        void clear_pin(Field field, int x, int y);
        bool is_pinned(Field field, int x, int y) const;
        float value(Field field, int x, int y) const;
        std::uint64_t tick_count() const noexcept { return tick_count_; }
        const WorldSimParams &params() const noexcept { return params_; }

        std::uint64_t hash() const;
        Json to_json() const;
        static WorldSim from_json(const Json &d);

    private:
        float noise(Field field, int x, int y) const;
        int index(int x, int y) const noexcept;
        void step();

        WorldSimParams params_;
        std::uint64_t tick_count_ = 0;
        std::vector<std::vector<float>> fields_{};
        std::vector<std::vector<float>> next_{};
        std::vector<std::vector<bool>> pins_{};
        bool configured_ = false;
    };

} // namespace aur::worldsim
