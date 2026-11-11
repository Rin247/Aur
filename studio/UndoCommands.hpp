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
        // Nested addressing used by MapLayer only: index is the layer,
        // row/col the cell. All other kinds address a flat array by cellIndex.
        int row = 0;
        int col = 0;

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

    protected:
        // Exposed for SnapshotCommand, which resolves the same regions.
        Json *targetArray(const RegionKey &region) const;

    private:
        void apply(bool after);

        RegionKey m_region;
        std::vector<CellDiff> m_diffs;
        Json *m_document = nullptr;
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
