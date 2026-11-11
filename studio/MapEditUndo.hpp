#pragma once

// MapEditUndo.hpp - the map editor's undo recorder: it collects the cells a
// stroke touched so the command carries per-cell diffs.

#include "UndoCommands.hpp"

#include <QString>
#include <QUndoStack>

#include <map>
#include <utility>

namespace aur::editor
{

    class MapEditUndo
    {
    public:
        // Opens a stroke on a map layer. Cells are recorded with noteCell().
        void beginLayer(int layer_index, const QString &label);
        // Opens a stroke on the entities array. An insert shifts every later
        // index, so this stroke keeps a whole-array snapshot.
        void beginEntities(Json &entities, const QString &label);
        // Records what a cell holds BEFORE the write. The first note for a
        // cell wins, so a stroke that repaints a cell keeps the original.
        void noteCell(Json &layer, int row, int col);
        // Closes the stroke and pushes it. False when nothing changed.
        bool commit(Json *document, QUndoStack *stack);
        void cancel();
        bool isOpen() const { return m_open; }

    private:
        RegionKey m_region;
        QString m_label;
        bool m_open = false;
        bool m_entities = false;
        std::map<std::pair<int, int>, Json> m_before;
        Json m_entities_before;
    };

} // namespace aur::editor
