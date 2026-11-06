#include "worldsim.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
        pins_.assign(kFieldCount, std::vector<bool>(cells, false));
    }

    void WorldSim::advance(int ticks)
    {
        tick_count_ += static_cast<std::uint64_t>(std::max(0, ticks));
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
        const int index = y * params_.width + x;
        if (index < 0 || index >= static_cast<int>(fields_[static_cast<int>(field)].size()))
            return 0.0f;
        return fields_[static_cast<int>(field)][static_cast<std::size_t>(index)];
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
