#include "BestiaryData.hpp"

#include "aur/gfx.hpp"

#include <algorithm>
#include <cctype>

namespace aur
{
    namespace bestiary
    {

        namespace
        {
            const std::vector<Field> &fieldTable()
            {
                static const std::vector<Field> table = {
                    {"name", "Name", false, 0, 0},
                    {"gfx", "Artwork", false, 0, 0},
                    {"look", "Look", false, 0, 0},
                    {"ai", "AI", false, 0, 0},
                    {"weapon", "Weapon", false, 0, 0},
                    {"hp", "HP", true, 1, 9999},
                    {"atk", "Attack", true, 0, 999},
                    {"def", "Defense", true, 0, 999},
                    {"spd", "Speed", true, 0, 999},
                    {"mov", "Movement", true, 0, 999},
                    {"skl", "Skill", true, 0, 999},
                    {"tspd", "Turn speed", true, 0, 999},
                    {"xp", "Experience", true, 0, 99999},
                };
                return table;
            }

            std::string slug(const std::string &name)
            {
                std::string out;
                for (char c : name)
                {
                    if (std::isalnum(static_cast<unsigned char>(c)))
                        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                    else if (!out.empty() && out.back() != '_')
                        out.push_back('_');
                }
                while (!out.empty() && out.back() == '_')
                    out.pop_back();
                return out.empty() ? std::string("creature") : out;
            }
        } // namespace

        const std::vector<Field> &fields()
        {
            return fieldTable();
        }

        bool isEditorField(const std::string &key)
        {
            const std::vector<Field> &table = fieldTable();
            return std::any_of(table.begin(), table.end(),
                               [&key](const Field &field) { return field.key == key; });
        }

        std::string label(const std::string &id, const Json &record)
        {
            if (record["name"].is_string() && !record["name"].as_string().empty())
                return record["name"].as_string();
            return id;
        }

        std::string suggestId(const JsonObject &enemies, const std::string &name)
        {
            const std::string base = slug(name);
            std::string id = base;
            int suffix = 2;
            while (enemies.contains(id))
                id = base + "_" + std::to_string(suffix++);
            return id;
        }

        Json newRecord(const std::string &id, const std::string &name)
        {
            Json record = Json(JsonObject{});
            record["name"] = name;
            record["gfx"] = id;
            record["look"] = id;
            record["ai"] = "chaser";
            record["weapon"] = "";
            record["hp"] = 10;
            record["atk"] = 1;
            record["def"] = 0;
            record["spd"] = 30;
            record["mov"] = 4;
            record["skl"] = 0;
            record["tspd"] = 4;
            record["xp"] = 1;
            return record;
        }

        std::string fieldText(const Json &record, const std::string &key)
        {
            const Json &value = record[key];
            if (value.is_string())
                return value.as_string();
            if (value.is_number())
                return std::to_string(value.as_int());
            return std::string();
        }

        bool artwork(const Project &project, const std::string &graphicId, int &width,
                     int &height, std::vector<unsigned char> &pixels)
        {
            width = 0;
            height = 0;
            pixels.clear();
            if (graphicId.empty())
                return false;
            const auto entry = project.graphics.find(graphicId);
            if (entry == project.graphics.end())
                return false;
            const Json graphic = entry->second;
            if (!graphic.contains("frames") || !graphic["frames"].is_object() ||
                graphic["frames"].as_object().empty())
                return false;
            const std::string frame = graphic["frames"].as_object().begin()->first;
            auto [frame_width, frame_height, data] = frame_rgba(graphic, frame);
            if (frame_width <= 0 || frame_height <= 0 || data.empty())
                return false;
            width = frame_width;
            height = frame_height;
            pixels = std::move(data);
            return true;
        }

        Json applyFields(const Json &record,
                         const std::vector<std::pair<std::string, std::string>> &values)
        {
            Json out = record;
            for (const auto &[key, text] : values)
            {
                if (!isEditorField(key))
                    continue;
                const Field *field = nullptr;
                for (const Field &candidate : fieldTable())
                {
                    if (candidate.key == key)
                    {
                        field = &candidate;
                        break;
                    }
                }
                if (!field)
                    continue;
                if (field->numeric)
                {
                    const int number = std::atoi(text.c_str());
                    out[key] = std::clamp(number, field->low, field->high);
                }
                else
                {
                    out[key] = text;
                }
            }
            return out;
        }

    } // namespace bestiary
} // namespace aur
