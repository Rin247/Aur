#include "core/UndoCommands.hpp"

namespace aur::editor
{

    const char *mapLayerName(int index)
    {
        switch (index)
        {
        case 0: return "ground";
        case 1: return "objects";
        case 2: return "above";
        default: return nullptr;
        }
    }

    Json *resolveRegion(const RegionKey &region, Json *document, Json *direct)
    {
        if (region.kind == RegionKey::Direct)
            return direct;
        if (!document || !document->is_object())
            return nullptr;
        switch (region.kind)
        {
        case RegionKey::MapLayer:
        {
            // The document is the map; layers.<name> is rows of rows and
            // the whole layer is the region, so a cell is (row, col).
            const char *layer_name = mapLayerName(region.index);
            if (!layer_name || !document->contains("layers"))
                return nullptr;
            Json &layers = (*document)["layers"];
            if (!layers.is_object() || !layers.contains(layer_name))
                return nullptr;
            Json &layer = layers[layer_name];
            return layer.is_array() ? &layer : nullptr;
        }
        case RegionKey::MapEntities:
        {
            if (!document->contains("entities") || !(*document)["entities"].is_array())
                return nullptr;
            return &(*document)["entities"];
        }
        case RegionKey::TileObjects:
        {
            // The document is the tileset; one cell is one tile object.
            if (!document->contains("tiles") || !(*document)["tiles"].is_array())
                return nullptr;
            return &(*document)["tiles"];
        }
        default:
            return nullptr;
        }
    }

    Json *CellCommand::targetArray(const RegionKey &region) const
    {
        return resolveRegion(region, m_document, m_direct);
    }

    void SnapshotCommand::redo()
    {
        if (Json *target = targetArray(region()))
            *target = m_after;
    }

    void SnapshotCommand::undo()
    {
        if (Json *target = targetArray(region()))
            *target = m_before;
    }

    void CellCommand::apply(bool after)
    {
        Json *target = targetArray(m_region);
        if (!target || !target->is_array())
            return;
        for (const CellDiff &diff : m_diffs)
        {
            const Json &value = after ? diff.after : diff.before;
            if (m_region.kind == RegionKey::MapLayer)
            {
                // The layer is rows of rows: the cell is (row, col).
                JsonArray &rows = target->as_array();
                if (diff.row < 0 || diff.row >= static_cast<int>(rows.size()))
                    continue;
                Json &row = rows[static_cast<size_t>(diff.row)];
                if (!row.is_array() || diff.col < 0 ||
                    diff.col >= static_cast<int>(row.as_array().size()))
                    continue;
                row.as_array()[static_cast<size_t>(diff.col)] = value;
                continue;
            }
            JsonArray &cells = target->as_array();
            if (diff.cellIndex < 0 || diff.cellIndex >= static_cast<int>(cells.size()))
                continue;
            cells[static_cast<size_t>(diff.cellIndex)] = value;
        }
    }

} // namespace aur::editor
