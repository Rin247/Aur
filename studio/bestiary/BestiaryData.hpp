#pragma once

// BestiaryData.hpp - the creature record, without a widget.
//
// The record shape is the project's: `db["enemies"][id]`. This module
// knows which fields the editor shows, what a new creature starts as,
// and how to write the editor's fields back without losing the rest.

#include <string>
#include <utility>
#include <vector>

#include "aur/json.hpp"
#include "aur/project.hpp"

namespace aur
{
    namespace bestiary
    {

        // One field the editor shows. The numeric ones are clamped to
        // their range when they are written.
        struct Field
        {
            std::string key;
            std::string label;
            bool numeric = false;
            int low = 0;
            int high = 999;
        };

        // The fields, in the order the form shows them.
        const std::vector<Field> &fields();

        // A field's key that the editor shows.
        bool isEditorField(const std::string &key);

        // A creature's name, or its id when it has none.
        std::string label(const std::string &id, const Json &record);

        // An id that is free, derived from the name.
        std::string suggestId(const JsonObject &enemies, const std::string &name);

        // A new creature's record: the defaults, and a name.
        Json newRecord(const std::string &id, const std::string &name);

        // The editor's fields as text, for the form to show.
        std::string fieldText(const Json &record, const std::string &key);

        // The creature's artwork, if the project has it: the width, the
        // height and the pixels. The pixels are the engine's own frame, so
        // the portrait is the picture the game draws.
        bool artwork(const Project &project, const std::string &graphicId, int &width,
                     int &height, std::vector<unsigned char> &pixels);

        // The editor's fields written back onto the record. The fields the
        // editor does not show are carried through untouched.
        Json applyFields(const Json &record,
                         const std::vector<std::pair<std::string, std::string>> &values);

    } // namespace bestiary
} // namespace aur
