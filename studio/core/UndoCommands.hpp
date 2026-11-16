#pragma once

// UndoCommands.hpp - one QUndoCommand shared by the map, pixel and tileset
// editors: a region key resolves to a Json array, then cells get rewritten.

#include <QUndoCommand>

#include "aur/json.hpp"

#include <utility>
#include <vector>

namespace aur::editor
{

    // One cell's before/after as JSON values: numbers on tile and pixel
    // arrays, whole objects on the entity and tile arrays.
    struct CellDiff
    {
        int cellIndex = 0; // index into the target array
        Json before;
        Json after;
        // A map layer is rows of rows, so a cell in one is a (row, col)
        // pair rather than a single index. Unused by the flat kinds.
        int row = 0;
        int col = 0;
    };

    // The region a command touches. Editors hand out these keys; the command
    // resolves each one against the live project document.
    struct RegionKey
    {
        int kind = 0;  // RegionKind
        int index = 0; // layer index / tile index

        enum RegionKind
        {
            // A map layer, resolved as the WHOLE layer (rows of rows).
            // A cell in it is addressed by the diff's row/col.
            MapLayer = 0,
            // The entities array of a map: one cell is one entity object.
            MapEntities = 1,
            // The tiles array of a tileset: one cell is one tile object.
            TileObjects = 2,
            // The document pointer IS the target array (used by editors
            // whose regions are keyed by name, like the pixel frames).
            Direct = 3
        };
    };

    // The layer names a map document stores, by index. One copy, so the
    // editor and the undo base cannot disagree about which is which.
    const char *mapLayerName(int index);

    // Resolves a region to the array its cells live in, or null when the
    // region is not there, so a stale command cannot crash a resized
    // document. `direct` is the target for the Direct kind.
    Json *resolveRegion(const RegionKey &region, Json *document, Json *direct = nullptr);

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
        // For kind Direct: the target array itself.
        void setDirect(Json *target) { m_direct = target; }

        void redo() override { apply(/*after=*/true); }
        void undo() override { apply(/*after=*/false); }

    protected:
        // The array a region's cells live in, or null when the region is
        // not there. Exposed for SnapshotCommand, which resolves the same
        // regions.
        Json *targetArray(const RegionKey &region) const;

    private:
        void apply(bool after);

        RegionKey m_region;
        std::vector<CellDiff> m_diffs;
        Json *m_document = nullptr;
        Json *m_direct = nullptr; // Direct kind: the target array
    };

    // A whole-region snapshot command: the transitional form the editors use
    // while their push sites convert. Stores what the old snapshot deques
    // stored, but on QUndoStack, so undo ownership, redo clearing and the
    // entry cap all move across at once.
    class SnapshotCommand : public CellCommand
    {
    public:
        explicit SnapshotCommand(const QString &text = QString()) : CellCommand(text) {}

        void setBefore(Json before) { m_before = std::move(before); }
        void setAfter(Json after) { m_after = std::move(after); }

        void redo() override;
        void undo() override;

    private:
        Json m_before;
        Json m_after;
    };

} // namespace aur::editor
