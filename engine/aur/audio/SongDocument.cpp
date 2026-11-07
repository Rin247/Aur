#include "aur/audio/SongDocument.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace aur
{
    namespace audio
    {

        namespace
        {

            constexpr int kDefaultBpm = 120;
            constexpr int kDefaultBeats = 4;

            int int_of(const Json &value, int fallback)
            {
                return value.is_number() ? value.as_int() : fallback;
            }

            Json &array_field(Json &song, const char *key)
            {
                if (!song.contains(key) || !song[key].is_array())
                    song[key] = JsonArray{};
                return song[key];
            }

        } // namespace

        std::vector<SongDocument::MapPoint> SongDocument::read_map(const Json &song,
                                                                   const char *key)
        {
            std::vector<MapPoint> points;
            if (!song.contains(key) || !song[key].is_array())
                return points;
            for (const Json &entry : song[key].as_array())
            {
                if (!entry.is_object())
                    continue;
                MapPoint p;
                p.step = int_of(entry.contains("step") ? Json(entry["step"]) : Json{}, 0);
                p.value_a = int_of(entry.contains("bpm")   ? Json(entry["bpm"])
                                   : entry.contains("num") ? Json(entry["num"])
                                                           : Json{},
                                   0);
                p.value_b = int_of(entry.contains("den") ? Json(entry["den"]) : Json{}, 0);
                points.push_back(p);
            }
            std::stable_sort(points.begin(), points.end(),
                             [](const MapPoint &a, const MapPoint &b)
                             { return a.step < b.step; });
            return points;
        }

        void SongDocument::write_map(Json &song, const char *key,
                                     const std::vector<MapPoint> &points, bool has_b)
        {
            JsonArray out;
            for (const MapPoint &p : points)
            {
                JsonObject entry{{"step", p.step}};
                if (has_b)
                {
                    entry["num"] = p.value_a;
                    entry["den"] = p.value_b;
                }
                else
                {
                    entry["bpm"] = p.value_a;
                }
                out.push_back(Json(std::move(entry)));
            }
            song[key] = Json(std::move(out));
        }

        int SongDocument::lookup(const std::vector<MapPoint> &points, int step,
                                 int fallback)
        {
            int value = fallback;
            for (const MapPoint &p : points)
            {
                if (p.step > step)
                    break;
                value = p.value_a;
            }
            return value;
        }

        void SongDocument::migrate(Json &song)
        {
            if (!song.is_object())
                return;
            if (song.contains("format") && song["format"].is_string() &&
                song["format"].as_string() == "2")
            {
                return; // already v2
            }
            song["format"] = "2";
            song["ticks_per_step"] = kTicksPerStep;
            song["steps_per_beat"] = steps_per_beat(song);

            // v1 "tempo" (song-level scalar) becomes the first tempo-map entry.
            const int v1Tempo = int_of(song.contains("tempo") ? Json(song["tempo"])
                                                              : Json{},
                                       kDefaultBpm);
            if (!song.contains("tempo_map") || !song["tempo_map"].is_array() ||
                song["tempo_map"].as_array().empty())
            {
                song["tempo_map"] = JsonArray{JsonObject{{"step", 0}, {"bpm", v1Tempo}}};
            }

            // v1 "beats" becomes the first time-signature entry.
            const int v1Beats = steps_per_beat(song);
            if (!song.contains("time_sig_map") || !song["time_sig_map"].is_array() ||
                song["time_sig_map"].as_array().empty())
            {
                song["time_sig_map"] =
                    JsonArray{JsonObject{{"step", 0}, {"num", v1Beats}, {"den", 4}}};
            }

            // v1 "loop_bar"·16 becomes loop.start; end defaults to bars·16.
            if (!song.contains("loop") || !song["loop"].is_object())
            {
                const int bars = int_of(song.contains("bars") ? Json(song["bars"])
                                                              : Json{},
                                        8);
                const int loopBar = int_of(song.contains("loop_bar") ? Json(song["loop_bar"])
                                                                     : Json{},
                                           0);
                song["loop"] = JsonObject{
                    {"start_step", loopBar * 16}, {"end_step", bars * 16}};
            }

            if (!song.contains("markers") || !song["markers"].is_array())
            {
                song["markers"] = JsonArray{};
            }
        }

        bool SongDocument::load(Json &song)
        {
            if (!song.is_object() || !song.contains("tracks") ||
                !song["tracks"].is_array())
            {
                return false;
            }
            migrate(song);
            return true;
        }

        bool SongDocument::save(const Json &song, const std::string &path)
        {
            // Backup the previous file before overwriting (contract 1).
            std::ifstream existing(path, std::ios::binary);
            if (existing)
            {
                std::ostringstream buffer;
                buffer << existing.rdbuf();
                const std::string backupPath = path + ".bak";
                std::ofstream backup(backupPath, std::ios::binary | std::ios::trunc);
                if (!backup)
                    return false;
                backup.write(buffer.str().data(),
                             static_cast<std::streamsize>(buffer.str().size()));
                if (!backup)
                    return false;
            }
            existing.close();

            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
                return false;
            const std::string text = aur::dump_json(song);
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            return static_cast<bool>(out);
        }

        int SongDocument::steps_per_beat(const Json &song)
        {
            if (song.contains("steps_per_beat") && song["steps_per_beat"].is_number())
                return song["steps_per_beat"].as_int();
            if (song.contains("beats") && song["beats"].is_number())
                return std::clamp(song["beats"].as_int(), 1, 32); // v1 "beats" = per-bar count? kept as-is
            return kDefaultBeats;
        }

        int SongDocument::bpm_at(const Json &song, int step)
        {
            return lookup(read_map(song, "tempo_map"), step, kDefaultBpm);
        }

        int SongDocument::beats_at(const Json &song, int step)
        {
            return lookup(read_map(song, "time_sig_map"), step, kDefaultBeats);
        }

        int SongDocument::loop_start(const Json &song)
        {
            if (song.contains("loop") && song["loop"].is_object() &&
                song["loop"].contains("start_step"))
            {
                return int_of(Json(song["loop"]["start_step"]), 0);
            }
            return int_of(song.contains("loop_bar") ? Json(song["loop_bar"]) : Json{}, 0) * 16;
        }

        int SongDocument::loop_end(const Json &song)
        {
            if (song.contains("loop") && song["loop"].is_object() &&
                song["loop"].contains("end_step"))
            {
                return int_of(Json(song["loop"]["end_step"]), 0);
            }
            const int bars = int_of(song.contains("bars") ? Json(song["bars"]) : Json{}, 8);
            return bars * 16;
        }

        void SongDocument::set_tempo_at(Json &song, int step, int bpm)
        {
            auto points = read_map(song, "tempo_map");
            points.erase(std::remove_if(points.begin(), points.end(),
                                        [step](const MapPoint &p)
                                        { return p.step == step; }),
                         points.end());
            points.push_back({step, bpm, 0});
            std::stable_sort(points.begin(), points.end(),
                             [](const MapPoint &a, const MapPoint &b)
                             { return a.step < b.step; });
            write_map(song, "tempo_map", points, false);
        }

        void SongDocument::set_time_sig_at(Json &song, int step, int num, int den)
        {
            auto points = read_map(song, "time_sig_map");
            points.erase(std::remove_if(points.begin(), points.end(),
                                        [step](const MapPoint &p)
                                        { return p.step == step; }),
                         points.end());
            points.push_back({step, num, den});
            std::stable_sort(points.begin(), points.end(),
                             [](const MapPoint &a, const MapPoint &b)
                             { return a.step < b.step; });
            write_map(song, "time_sig_map", points, true);
        }

    } // namespace audio
} // namespace aur
