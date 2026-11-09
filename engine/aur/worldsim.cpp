#include "worldsim.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace aur::worldsim
{

    namespace
    {

        std::uint64_t fnv1a(const std::uint8_t *data, std::size_t size, std::uint64_t basis)
        {
            std::uint64_t hash = basis;
            for (std::size_t i = 0; i < size; ++i)
            {
                hash ^= data[i];
                hash *= 0x100000001b3ULL;
            }
            return hash;
        }

        std::uint64_t mix64(std::uint64_t v)
        {
            v ^= v >> 33;
            v *= 0xff51afd7ed558ccdULL;
            v ^= v >> 33;
            v *= 0xc4ceb9fe1a85ec53ULL;
            v ^= v >> 33;
            return v;
        }

    } // namespace

    WorldSim::WorldSim(const WorldSimParams &params) : params_(params)
    {
        const std::size_t cells =
            static_cast<std::size_t>(params_.width) * static_cast<std::size_t>(params_.height);
        fields_.assign(kFieldCount, std::vector<float>(cells, 0.0f));
        next_.assign(kFieldCount, std::vector<float>(cells, 0.0f));
        pins_.assign(kFieldCount, std::vector<bool>(cells, false));
    }

    int WorldSim::index(int x, int y) const noexcept
    {
        return y * params_.width + x;
    }

    float WorldSim::noise(Field field, int x, int y) const
    {
        const std::uint64_t cell =
            (static_cast<std::uint64_t>(static_cast<unsigned>(y)) << 32) |
            static_cast<std::uint32_t>(x);
        return static_cast<float>(mix64(cell ^ params_.seed ^
                                        (0x9e3779b97f4a7c15ULL * static_cast<std::uint64_t>(field))) >>
                                  11) * (1.0f / 9007199254740992.0f);
    }

    void WorldSim::set_heights(const std::vector<float> &heights)
    {
        const std::size_t cells =
            static_cast<std::size_t>(params_.width) * static_cast<std::size_t>(params_.height);
        if (heights.size() != cells) return;
        fields_[static_cast<int>(Field::Temperature)] = heights;
        configured_ = true;
    }

    void WorldSim::advance(int ticks)
    {
        for (int t = 0; t < std::max(0, ticks); ++t) step();
    }

    void WorldSim::step()
    {
        const int w = params_.width;
        const int h = params_.height;

        auto read = [this, w, h](Field field, int x, int y) -> float {
            const int i = index(aur::wrapX(x, w), y);
            return fields_[static_cast<int>(field)][static_cast<std::size_t>(i)];
        };

        // Pass 1: diffusion fields (temperature, moisture) — 3x3 wrap-aware mean.
        for (Field field : {Field::Temperature, Field::Moisture})
        {
            const auto &source = fields_[static_cast<int>(field)];
            auto &target = next_[static_cast<int>(field)];
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    const int i = index(x, y);
                    const std::size_t si = static_cast<std::size_t>(i);
                    if (pins_[static_cast<int>(field)][si])
                    {
                        target[si] = source[si];
                        continue;
                    }
                    double mix = 0.0;
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        const int ny = y + dy;
                        if (ny < 0 || ny >= h) continue;
                        for (int dx = -1; dx <= 1; ++dx)
                            mix += source[static_cast<std::size_t>(index(aur::wrapX(x + dx, w), ny))];
                    }
                    const double blended = mix / 9.0;
                    target[si] = static_cast<float>(source[si] +
                                                    (blended - source[si]) * params_.diffusion_rate);
                }
            }
        }

        // Pass 2: wind advection of cloud coverage, then cloud growth/decay.
        const auto &cloud = fields_[static_cast<int>(Field::Cloud)];
        const auto &moisture = fields_[static_cast<int>(Field::Moisture)];
        {
            auto &target = next_[static_cast<int>(Field::Cloud)];
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    const int i = index(x, y);
                    const std::size_t si = static_cast<std::size_t>(i);
                    if (pins_[static_cast<int>(Field::Cloud)][si])
                    {
                        target[si] = cloud[si];
                        continue;
                    }
                    const double u = read(Field::WindU, x, y);
                    const double v = read(Field::WindV, x, y);
                    const double sx = aur::wrapX(
                        x - static_cast<int>(u * params_.wind_step * 16.0), w);
                    const int sy = y - static_cast<int>(v * params_.wind_step * 16.0);
                    const double upstream = sy >= 0 && sy < h
                        ? cloud[static_cast<std::size_t>(index(static_cast<int>(sx), sy))]
                        : 0.0;
                    double c = upstream + (moisture[si] - 0.5) * params_.cloud_growth -
                               params_.cloud_decay;
                    target[si] = static_cast<float>(std::clamp(c, 0.0, 1.0));
                }
            }
        }

        // Pass 3: precipitation and snow from the *new* cloud field.
        const auto &temperature = fields_[static_cast<int>(Field::Temperature)];
        const auto &new_cloud = next_[static_cast<int>(Field::Cloud)];
        {
            auto &next_rain = next_[static_cast<int>(Field::Rain)];
            auto &next_snow = next_[static_cast<int>(Field::Snowpack)];
            const auto &old_snow = fields_[static_cast<int>(Field::Snowpack)];
            for (int y = 0; y < h; ++y)
            {
                for (int x = 0; x < w; ++x)
                {
                    const int i = index(x, y);
                    const std::size_t si = static_cast<std::size_t>(i);
                    const double wet = new_cloud[si];
                    const double temp = temperature[si];
                    const double frozen = temp < params_.snow_threshold;
                    const double fall = wet * params_.rain_rate;

                    if (pins_[static_cast<int>(Field::Rain)][si])
                        next_rain[si] = fields_[static_cast<int>(Field::Rain)][si];
                    else
                        next_rain[si] = static_cast<float>(frozen ? 0.0 : fall);

                    if (pins_[static_cast<int>(Field::Snowpack)][si])
                        next_snow[si] = old_snow[si];
                    else
                        next_snow[si] = static_cast<float>(
                            frozen ? old_snow[si] + fall
                                   : std::max(0.0, old_snow[si] - params_.snow_melt));
                }
            }
        }

        // Wind fields relax toward a latitude-driven base flow (no external input).
        for (int field_index : {static_cast<int>(Field::WindU), static_cast<int>(Field::WindV)})
        {
            const auto &source = fields_[static_cast<std::size_t>(field_index)];
            auto &target = next_[static_cast<std::size_t>(field_index)];
            const Field field = static_cast<Field>(field_index);
            const double sign = field == Field::WindU ? 1.0 : -1.0;
            for (int y = 0; y < h; ++y)
            {
                const double latitude = (h > 1 ? static_cast<double>(y) / (h - 1) : 0.5) * 2.0 - 1.0;
                const double base = sign * (1.0 - std::abs(latitude)) *
                                    (field == Field::WindU ? 0.5 : 0.25);
                for (int x = 0; x < w; ++x)
                {
                    const int i = index(x, y);
                    const std::size_t si = static_cast<std::size_t>(i);
                    if (pins_[static_cast<int>(field)][si])
                    {
                        target[si] = source[si];
                        continue;
                    }
                    target[si] = static_cast<float>(source[si] +
                                                    (base - source[si]) * params_.wind_step);
                }
            }
        }

        fields_.swap(next_);
        ++tick_count_;
    }

    void WorldSim::pin(Field field, int x, int y, float value)
    {
        const int index = y * params_.width + x;
        if (index < 0 || index >= static_cast<int>(fields_[static_cast<int>(field)].size()))
            return;
        fields_[static_cast<int>(field)][static_cast<std::size_t>(index)] = value;
        pins_[static_cast<int>(field)][static_cast<std::size_t>(index)] = true;
    }

    void WorldSim::clear_pin(Field field, int x, int y)
    {
        const int index = y * params_.width + x;
        if (index < 0 || index >= static_cast<int>(pins_[static_cast<int>(field)].size()))
            return;
        pins_[static_cast<int>(field)][static_cast<std::size_t>(index)] = false;
    }

    bool WorldSim::is_pinned(Field field, int x, int y) const
    {
        const int index = y * params_.width + x;
        if (index < 0 || index >= static_cast<int>(pins_[static_cast<int>(field)].size()))
            return false;
        return pins_[static_cast<int>(field)][static_cast<std::size_t>(index)];
    }

    float WorldSim::value(Field field, int x, int y) const
    {
        const int wx = aur::wrapX(x, params_.width);
        if (y < 0 || y >= params_.height) return 0.0f;
        return fields_[static_cast<int>(field)][static_cast<std::size_t>(index(wx, y))];
    }

    std::uint64_t WorldSim::hash() const
    {
        std::uint64_t basis = fnv1a(reinterpret_cast<const std::uint8_t *>(&tick_count_),
                                    sizeof(tick_count_), 0xcbf29ce484222325ULL);
        basis = fnv1a(reinterpret_cast<const std::uint8_t *>(&params_.seed),
                      sizeof(params_.seed), basis);
        for (const auto &field : fields_)
        {
            basis = fnv1a(reinterpret_cast<const std::uint8_t *>(field.data()),
                          field.size() * sizeof(float), basis);
        }
        for (const auto &field_pins : pins_)
        {
            for (bool p : field_pins)
                basis = fnv1a(reinterpret_cast<const std::uint8_t *>(&p), 1, basis);
        }
        return basis;
    }

    Json WorldSim::to_json() const
    {
        JsonObject d;
        d["width"] = params_.width;
        d["height"] = params_.height;
        {
            char seed_hex[17];
            std::snprintf(seed_hex, sizeof(seed_hex), "%016llx",
                          static_cast<unsigned long long>(params_.seed));
            d["seed"] = std::string(seed_hex);
        }
        {
            char tick_hex[17];
            std::snprintf(tick_hex, sizeof(tick_hex), "%016llx",
                          static_cast<unsigned long long>(tick_count_));
            d["ticks"] = std::string(tick_hex);
        }
        JsonArray field_arrays;
        for (const auto &field : fields_)
        {
            JsonArray values;
            values.reserve(field.size());
            for (float v : field)
                values.emplace_back(static_cast<double>(v));
            field_arrays.emplace_back(Json(std::move(values)));
        }
        d["fields"] = Json(std::move(field_arrays));
        JsonArray pin_arrays;
        for (const auto &field_pins : pins_)
        {
            JsonArray flags;
            flags.reserve(field_pins.size());
            for (bool p : field_pins) flags.emplace_back(p);
            pin_arrays.emplace_back(Json(std::move(flags)));
        }
        d["pins"] = Json(std::move(pin_arrays));
        return Json(d);
    }

    WorldSim WorldSim::from_json(const Json &d)
    {
        WorldSimParams params;
        params.width = d["width"].as_int();
        params.height = d["height"].as_int();
        params.seed = std::strtoull(d["seed"].as_string().c_str(), nullptr, 16);
        if (d.contains("seconds_per_tick"))
            params.seconds_per_tick = d["seconds_per_tick"].as_number();
        WorldSim sim(params);
        sim.tick_count_ = std::strtoull(d["ticks"].as_string().c_str(), nullptr, 16);
        const auto &field_arrays = d["fields"].as_array();
        for (int f = 0; f < kFieldCount && f < static_cast<int>(field_arrays.size()); ++f)
        {
            const auto &values = field_arrays[static_cast<std::size_t>(f)].as_array();
            for (std::size_t i = 0; i < sim.fields_[static_cast<std::size_t>(f)].size() &&
                                    i < values.size();
                 ++i)
            {
                sim.fields_[static_cast<std::size_t>(f)][i] =
                    static_cast<float>(values[i].as_number());
            }
        }
        if (d.contains("pins"))
        {
            const auto &pin_arrays = d["pins"].as_array();
            for (int f = 0; f < kFieldCount && f < static_cast<int>(pin_arrays.size()); ++f)
            {
                const auto &flags = pin_arrays[static_cast<std::size_t>(f)].as_array();
                for (std::size_t i = 0; i < sim.pins_[static_cast<std::size_t>(f)].size() &&
                                        i < flags.size();
                     ++i)
                {
                    sim.pins_[static_cast<std::size_t>(f)][i] = flags[i].as_bool();
                }
            }
        }
        return sim;
    }

} // namespace aur::worldsim
