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

        int SongDocument::note_velocity(const Json &note)
        {
            if (!note.is_array() || note.as_array().size() < 4)
                return 90;
            const Json &velocity = note.as_array()[3];
            return velocity.is_number() ? std::clamp(velocity.as_int(), 0, 127) : 90;
        }

        void SongDocument::set_note_velocity(Json &note, int velocity)
        {
            if (!note.is_array() || note.as_array().size() < 4)
                return;
            note.as_array()[3] = std::clamp(velocity, 0, 127);
        }

        const Json *SongDocument::note_props(const Json &note)
        {
            if (!note.is_array() || note.as_array().size() < 5)
                return nullptr;
            const Json &props = note.as_array()[4];
            return props.is_object() ? &props : nullptr;
        }

        void SongDocument::set_note_prop(Json &note, const char *key, int value)
        {
            if (!note.is_array())
                return;
            JsonArray &items = note.as_array();
            while (items.size() < 4)
                items.push_back(Json(0));
            if (items.size() < 5 || !items[4].is_object())
                items.push_back(Json(JsonObject{}));
            items[4].as_object()[key] = value;
        }

        void SongDocument::clear_note_props(Json &note)
        {
            if (!note.is_array() || note.as_array().size() < 5)
                return;
            note.as_array().erase(note.as_array().begin() + 4);
        }

        const Json *SongDocument::lane_at(const Json &track, int lane)
        {
            if (!track.contains("automation") || !track["automation"].is_array())
                return nullptr;
            for (const Json &entry : track["automation"].as_array())
            {
                if (entry.is_object() && entry.contains("cc") &&
                    entry["cc"].is_number() && entry["cc"].as_int() == lane)
                    return &entry;
            }
            return nullptr;
        }

        Json *SongDocument::lane_at_mut(Json &track, int lane)
        {
            if (!track.contains("automation") || !track["automation"].is_array())
                return nullptr;
            for (Json &entry : track["automation"].as_array())
            {
                if (entry.is_object() && entry.contains("cc") &&
                    entry["cc"].is_number() && entry["cc"].as_int() == lane)
                    return &entry;
            }
            return nullptr;
        }

        Json *SongDocument::create_lane(Json &track, int lane)
        {
            if (Json *existing = lane_at_mut(track, lane))
                return existing;
            if (!track.is_object())
                return nullptr;
            if (!track.contains("automation") || !track["automation"].is_array())
                track["automation"] = JsonArray{};
            track["automation"].as_array().push_back(
                Json(JsonObject{{"cc", lane}, {"points", JsonArray{}}}));
            return &track["automation"].as_array().back();
        }

        std::vector<SongDocument::LanePoint> SongDocument::lane_points(const Json &track,
                                                                      int lane)
        {
            std::vector<LanePoint> points;
            const Json *entry = lane_at(track, lane);
            if (!entry || !entry->contains("points") || !(*entry)["points"].is_array())
                return points;
            for (const Json &point : (*entry)["points"].as_array())
            {
                if (!point.is_object() || !point.contains("step") ||
                    !point.contains("value"))
                    continue;
                LanePoint p{0, 0};
                p.step = point["step"].is_number() ? point["step"].as_int() : 0;
                p.value = point["value"].is_number() ? point["value"].as_int() : 0;
                points.push_back(p);
            }
            std::stable_sort(points.begin(), points.end(),
                             [](const LanePoint &a, const LanePoint &b)
                             { return a.step < b.step; });
            return points;
        }

        void SongDocument::set_lane_point(Json &track, int lane, int step, int value)
        {
            Json *entry = lane_at_mut(track, lane);
            if (!entry)
                entry = create_lane(track, lane);
            if (!entry)
                return;
            if (!entry->contains("points") || !(*entry)["points"].is_array())
                (*entry)["points"] = JsonArray{};
            JsonArray &points = (*entry)["points"].as_array();
            for (Json &point : points)
            {
                if (point.is_object() && point.contains("step") &&
                    point["step"].is_number() && point["step"].as_int() == step)
                {
                    point["value"] = std::clamp(value, 0, 127);
                    return;
                }
            }
            points.push_back(Json(JsonObject{{"step", std::max(0, step)},
                                             {"value", std::clamp(value, 0, 127)}}));
            std::stable_sort(points.begin(), points.end(),
                             [](const Json &a, const Json &b)
                             {
                                 const int sa = a.is_object() && a.contains("step") &&
                                                        a["step"].is_number()
                                                    ? a["step"].as_int() : 0;
                                 const int sb = b.is_object() && b.contains("step") &&
                                                        b["step"].is_number()
                                                    ? b["step"].as_int() : 0;
                                 return sa < sb;
                             });
        }

        void SongDocument::clear_lane_point(Json &track, int lane, int step)
        {
            Json *entry = lane_at_mut(track, lane);
            if (!entry || !entry->contains("points") || !(*entry)["points"].is_array())
                return;
            JsonArray &points = (*entry)["points"].as_array();
            points.erase(std::remove_if(points.begin(), points.end(),
                                        [step](const Json &point)
                                        {
                                            return point.is_object() &&
                                                   point.contains("step") &&
                                                   point["step"].is_number() &&
                                                   point["step"].as_int() == step;
                                        }),
                         points.end());
        }

        int SongDocument::lane_value_at(const Json &track, int lane, int step, int fallback)
        {
            const std::vector<LanePoint> points = lane_points(track, lane);
            int value = fallback;
            for (const LanePoint &p : points)
            {
                if (p.step > step)
                    break;
                value = p.value;
            }
            return value;
        }

        const Json *SongDocument::clip_at(const Json &song, int sceneIndex,
                                          int trackIndex)
        {
            if (sceneIndex < 0 || trackIndex < 0 || !song.contains("scenes") ||
                !song["scenes"].is_array() ||
                sceneIndex >= static_cast<int>(song["scenes"].as_array().size()))
                return nullptr;
            const Json &scene = song["scenes"][static_cast<size_t>(sceneIndex)];
            if (!scene.contains("clips") || !scene["clips"].is_array() ||
                trackIndex >= static_cast<int>(scene["clips"].as_array().size()))
                return nullptr;
            const Json &clip = scene["clips"][static_cast<size_t>(trackIndex)];
            return clip.is_object() ? &clip : nullptr;
        }

        Json *SongDocument::clip_at_mut(Json &song, int sceneIndex, int trackIndex)
        {
            if (sceneIndex < 0 || trackIndex < 0 || !song.contains("scenes") ||
                !song["scenes"].is_array() ||
                sceneIndex >= static_cast<int>(song["scenes"].as_array().size()))
                return nullptr;
            Json &scene = song["scenes"][static_cast<size_t>(sceneIndex)];
            if (!scene.contains("clips") || !scene["clips"].is_array() ||
                trackIndex >= static_cast<int>(scene["clips"].as_array().size()))
                return nullptr;
            Json &clip = scene["clips"][static_cast<size_t>(trackIndex)];
            return clip.is_object() ? &clip : nullptr;
        }

        SongDocument::ClipInfo SongDocument::clip_info(const Json &clip,
                                                       const std::string &fallbackName)
        {
            ClipInfo info;
            info.name = fallbackName;
            if (!clip.is_object())
                return info;
            if (clip.contains("name") && clip["name"].is_string() &&
                !clip["name"].as_string().empty())
                info.name = clip["name"].as_string();
            if (clip.contains("start") && clip["start"].is_number())
                info.start = std::max(0, clip["start"].as_int());
            if (clip.contains("length") && clip["length"].is_number())
                info.length = std::clamp(clip["length"].as_int(), 1, 2048);
            if (clip.contains("muted") && clip["muted"].is_bool())
                info.muted = clip["muted"].as_bool();
            if (clip.contains("color") && clip["color"].is_number())
                info.color = clip["color"].as_int();
            return info;
        }

        void SongDocument::set_clip_info(Json &clip, const ClipInfo &info)
        {
            if (!clip.is_object())
                return;
            clip["name"] = info.name;
            clip["start"] = std::max(0, info.start);
            clip["length"] = std::clamp(info.length, 1, 2048);
            clip["muted"] = info.muted;
            if (info.color >= 0)
                clip["color"] = info.color;
        }

    } // namespace audio
} // namespace aur
