#pragma once

#include "aur/audio/SongDocument.hpp"
#include "aur/json.hpp"

#include <set>
#include <vector>

namespace aur::editor
{

    // Maps between note array indices and stable note ids (P5). Selection is
    // stored by index while dragging (fast), but converted to ids whenever
    // the notes array mutates outside a drag (undo/redo, paste, song swap)
    // so the selection survives re-sorting and insertion.
    class NoteIndex
    {
    public:
        // Ids for the track's notes, in array order; un-id'd notes map to -1.
        static std::vector<int> ids(const Json &track)
        {
            std::vector<int> result;
            if (!track.is_object() || !track.contains("notes") ||
                !track["notes"].is_array())
                return result;
            for (const Json &note : track["notes"].as_array())
                result.push_back(audio::SongDocument::note_id(note));
            return result;
        }

        // Convert a set of indices to ids using a snapshot mapping.
        static std::set<int> toIds(const std::set<int> &indices,
                                   const std::vector<int> &indexToId)
        {
            std::set<int> ids;
            for (int index : indices)
            {
                if (index >= 0 && index < static_cast<int>(indexToId.size()) &&
                    indexToId[static_cast<size_t>(index)] >= 0)
                    ids.insert(indexToId[static_cast<size_t>(index)]);
            }
            return ids;
        }

        // Convert ids back to indices under the current mapping; ids that
        // vanished (deleted notes) are dropped.
        static std::set<int> toIndices(const std::set<int> &idsIn,
                                       const std::vector<int> &indexToId)
        {
            std::set<int> indices;
            for (size_t index = 0; index < indexToId.size(); ++index)
            {
                if (indexToId[index] >= 0 && idsIn.count(indexToId[index]))
                    indices.insert(static_cast<int>(index));
            }
            return indices;
        }
    };

} // namespace aur::editor
