#pragma once

// RecordFields.hpp - the fields of a record, as data.
//
// One row per field: the key, the label, the kind and the range. Adding
// a field to a kind is a row, not code. The layer is Qt-free, so the
// test does not need a widget.

#include <string>
#include <utility>
#include <vector>

#include "aur/json.hpp"

namespace aur
{
    namespace records
    {

        enum class FieldKind
        {
            Text,     // a line of text
            Number,   // a whole number
            Fraction, // a decimal number
            Group,    // a nested set of numbers (a character's growth)
            Pair      // two numbers (a weapon's range)
        };

        struct Field
        {
            std::string key;
            std::string label;
            FieldKind kind = FieldKind::Text;
            int low = 0;
            int high = 999;
            // The sub-fields of a group, or the two of a pair.
            std::vector<std::pair<std::string, int>> parts;
        };

        // The fields of a kind of record ("actors", "classes", "items",
        // "weapons", "skills"). An unknown kind has no fields.
        const std::vector<Field> &fieldsFor(const std::string &kind);

        // A new record's defaults: every field the editor shows, and
        // nothing else.
        Json newRecord(const std::string &kind, const std::string &id, const std::string &name);

        // The field's value as text, for the form. A group or a pair is
        // read part by part, so the form's value is one string per part.
        std::string fieldText(const Json &record, const Field &field, const std::string &part = "");

        // The write: the fields the editor shows, with the rest carried
        // through untouched. The values are keyed by the field's path
        // ("name", "growth.hp", "range.0").
        Json applyFields(const std::string &kind, const Json &record,
                         const std::vector<std::pair<std::string, std::string>> &values);

        // The number in a field's text: a comma is a decimal point, so a
        // locale that writes "0,5" is read the same way.
        double parseNumber(const std::string &text);

    } // namespace records
} // namespace aur
