#include "RecordFields.hpp"

#include <algorithm>
#include <cstdlib>

namespace aur
{
    namespace records
    {

        namespace
        {
            const std::vector<Field> &actorFields()
            {
                static const std::vector<Field> table = {
                    {"name", "Name", FieldKind::Text, 0, 0, {}},
                    {"char", "Character", FieldKind::Text, 0, 0, {}},
                    {"race", "Race", FieldKind::Text, 0, 0, {}},
                    {"class", "Class", FieldKind::Text, 0, 0, {}},
                    {"level", "Level", FieldKind::Number, 1, 99, {}},
                    {"hp", "HP", FieldKind::Number, 1, 9999, {}},
                    {"mp", "MP", FieldKind::Number, 0, 9999, {}},
                    {"str", "Strength", FieldKind::Number, 0, 999, {}},
                    {"def", "Defense", FieldKind::Number, 0, 999, {}},
                    {"mag", "Magic", FieldKind::Number, 0, 999, {}},
                    {"spd", "Speed", FieldKind::Number, 0, 999, {}},
                    {"skill", "Skill", FieldKind::Text, 0, 0, {}},
                    {"weapon", "Weapon", FieldKind::Text, 0, 0, {}},
                    {"growth", "Growth", FieldKind::Group, 0, 0,
                     {{"hp", 999}, {"mp", 999}, {"str", 99}, {"def", 99}, {"mag", 99}, {"spd", 99}}},
                };
                return table;
            }

            const std::vector<Field> &classFields()
            {
                static const std::vector<Field> table = {
                    {"name", "Class Name", FieldKind::Text, 0, 0, {}},
                    {"move", "Movement", FieldKind::Number, 0, 99, {}},
                };
                return table;
            }

            const std::vector<Field> &raceFields()
            {
                static const std::vector<Field> table = {
                    {"name", "Name", FieldKind::Text, 0, 0, {}},
                    {"category", "Category", FieldKind::Text, 0, 0, {}},
                    {"lifespan", "Lifespan", FieldKind::Text, 0, 0, {}},
                    {"maturation", "Maturation", FieldKind::Text, 0, 0, {}},
                    {"habitat", "Habitat", FieldKind::Text, 0, 0, {}},
                    {"appearance", "Appearance", FieldKind::Text, 0, 0, {}},
                    {"adaptations", "Adaptations", FieldKind::Text, 0, 0, {}},
                    {"culture", "Culture and society", FieldKind::Text, 0, 0, {}},
                    {"naming", "Naming", FieldKind::Text, 0, 0, {}},
                    {"combat_role", "Gameplay / combat role", FieldKind::Text, 0, 0, {}},
                    {"notes", "Additional notes", FieldKind::Text, 0, 0, {}},
                };
                return table;
            }

            const std::vector<Field> &itemFields()
            {
                static const std::vector<Field> table = {
                    {"name", "Item Name", FieldKind::Text, 0, 0, {}},
                    {"type", "Type", FieldKind::Text, 0, 0, {}},
                    {"value", "Value", FieldKind::Number, 0, 9999, {}},
                    {"price", "Price", FieldKind::Number, 0, 9999, {}},
                };
                return table;
            }

            const std::vector<Field> &weaponFields()
            {
                static const std::vector<Field> table = {
                    {"name", "Weapon Name", FieldKind::Text, 0, 0, {}},
                    {"kind", "Kind", FieldKind::Text, 0, 0, {}},
                    {"might", "Might", FieldKind::Number, 0, 999, {}},
                    {"reach", "Reach", FieldKind::Number, 0, 999, {}},
                    {"arc", "Arc", FieldKind::Number, 0, 360, {}},
                    {"hit", "Hit", FieldKind::Number, 0, 100, {}},
                    {"cd", "Cooldown", FieldKind::Fraction, 0, 100, {}},
                    {"range", "Range", FieldKind::Pair, 0, 999, {{"near", 999}, {"far", 999}}},
                };
                return table;
            }

            const std::vector<Field> &skillFields()
            {
                static const std::vector<Field> table = {
                    {"name", "Skill Name", FieldKind::Text, 0, 0, {}},
                    {"kind", "Kind", FieldKind::Text, 0, 0, {}},
                    {"mp", "MP Cost", FieldKind::Number, 0, 999, {}},
                    {"power", "Power", FieldKind::Fraction, 0, 999, {}},
                    {"radius", "Radius", FieldKind::Number, 0, 999, {}},
                };
                return table;
            }

            const std::vector<Field> &emptyFields()
            {
                static const std::vector<Field> table;
                return table;
            }

            double defaultNumber(const Field &field)
            {
                switch (field.kind)
                {
                case FieldKind::Number:
                    return field.low > 1 ? field.low : 1;
                case FieldKind::Fraction:
                    return 1.0;
                default:
                    return 0.0;
                }
            }
        } // namespace

        const std::vector<Field> &fieldsFor(const std::string &kind)
        {
            if (kind == "actors") return actorFields();
            if (kind == "classes") return classFields();
            if (kind == "races") return raceFields();
            if (kind == "items") return itemFields();
            if (kind == "weapons") return weaponFields();
            if (kind == "skills") return skillFields();
            return emptyFields();
        }

        Json newRecord(const std::string &kind, const std::string &id, const std::string &name)
        {
            Json record = Json(JsonObject{});
            for (const Field &field : fieldsFor(kind))
            {
                if (field.key == "name")
                {
                    record[field.key] = name;
                    continue;
                }
                switch (field.kind)
                {
                case FieldKind::Group:
                {
                    Json group = Json(JsonObject{});
                    for (const auto &[part, high] : field.parts)
                        group[part] = 0;
                    record[field.key] = group;
                    break;
                }
                case FieldKind::Pair:
                {
                    Json pair = Json(JsonArray{});
                    pair.as_array().push_back(Json(0));
                    pair.as_array().push_back(Json(0));
                    record[field.key] = pair;
                    break;
                }
                case FieldKind::Fraction:
                    record[field.key] = 1.0;
                    break;
                case FieldKind::Number:
                    record[field.key] = static_cast<int>(defaultNumber(field));
                    break;
                case FieldKind::Text:
                default:
                    record[field.key] = id;
                    break;
                }
            }
            return record;
        }

        std::string fieldText(const Json &record, const Field &field, const std::string &part)
        {
            const Json &value = record[field.key];
            if (field.kind == FieldKind::Group && !part.empty())
            {
                const Json &sub = value[part];
                if (sub.is_number())
                    return std::to_string(sub.as_int());
                return "0";
            }
            if (field.kind == FieldKind::Pair && !part.empty())
            {
                if (value.is_array())
                {
                    const size_t index = part == "near" ? 0 : 1;
                    if (index < value.as_array().size() && value.as_array()[index].is_number())
                        return std::to_string(value.as_array()[index].as_int());
                }
                return "0";
            }
            if (value.is_number())
                return std::to_string(value.as_int());
            if (value.is_string())
                return value.as_string();
            return std::string();
        }

        double parseNumber(const std::string &text)
        {
            std::string plain = text;
            std::replace(plain.begin(), plain.end(), ',', '.');
            return std::atof(plain.c_str());
        }

        Json applyFields(const std::string &kind, const Json &record,
                         const std::vector<std::pair<std::string, std::string>> &values)
        {
            Json out = record;
            const std::vector<Field> &table = fieldsFor(kind);
            for (const auto &[path, text] : values)
            {
                const size_t dot = path.find('.');
                const std::string key = dot == std::string::npos ? path : path.substr(0, dot);
                const std::string part = dot == std::string::npos ? std::string()
                                                                  : path.substr(dot + 1);
                const Field *field = nullptr;
                for (const Field &candidate : table)
                {
                    if (candidate.key == key)
                    {
                        field = &candidate;
                        break;
                    }
                }
                if (!field)
                    continue;
                if (field->kind == FieldKind::Group && !part.empty())
                {
                    Json group = out[key];
                    group[part] = static_cast<int>(parseNumber(text));
                    out[key] = group;
                    continue;
                }
                if (field->kind == FieldKind::Pair && !part.empty())
                {
                    Json pair = out[key];
                    if (!pair.is_array())
                        pair = Json(JsonArray{});
                    const size_t index = part == "near" ? 0 : 1;
                    while (pair.as_array().size() <= index)
                        pair.as_array().push_back(Json(0));
                    pair.as_array()[index] = static_cast<int>(parseNumber(text));
                    out[key] = pair;
                    continue;
                }
                if (field->kind == FieldKind::Number)
                {
                    out[key] = std::clamp(static_cast<int>(parseNumber(text)), field->low,
                                           field->high);
                    continue;
                }
                if (field->kind == FieldKind::Fraction)
                {
                    out[key] = parseNumber(text);
                    continue;
                }
                out[key] = text;
            }
            return out;
        }

    } // namespace records
} // namespace aur
