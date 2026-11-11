#pragma once

// UndoCommands.hpp - one QUndoCommand shared by the map, pixel and tileset
// editors: a region key resolves to a Json array, then cells get rewritten.

#include <QUndoCommand>

#include "aur/json.hpp"

#include <utility>
#include <vector>

namespace aur::editor
{

    // One cell's before/after as JSON values: numbers on tile/pixel/flag
    // arrays, whole objects on the entity array.
    struct CellDiff
    {
        int cellIndex = 0; // index into the target array
        Json before;
        Json after;
    };

    // The region a command touches. Editors hand out these keys; the command
    // resolves each one against the live project document.
    struct RegionKey
    {
        int kind = 0;  // RegionKind
        int index = 0; // layer index / frame index / tile index
        int slot = 0;  // free slot for editors that need a second index

        enum RegionKind
        {
            MapLayer = 0,
            MapEntities = 1,
            GraphicPixels = 2,
            TileFlags = 3
        };
    };

    // One undo command over a set of cells in one region. The document is
    // borrowed, not owned; the owner must outlive the stack.
    class CellCommand : public QUndoCommand
    {
    public:
        explicit CellCommand(const QString &text = QString()) : QUndoCommand(text) {}

        void addDiff(CellDiff diff) { m_diffs.push_back(std::move(diff)); }
        const std::vector<CellDiff> &diffs() const { return m_diffs; }
        void setRegion(RegionKey region) { m_region = region; }
        RegionKey region() const { return m_region; }
        void setDocument(Json *document) { m_document = document; }

        void redo() override { apply(/*after=*/true); }
        void undo() override { apply(/*after=*/false); }

    private:
        Json *targetArray(const RegionKey &region) const;
        void apply(bool after);

        RegionKey m_region;
        std::vector<CellDiff> m_diffs;
        Json *m_document = nullptr;
    };

} // namespace aur::editor
