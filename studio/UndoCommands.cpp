#include "UndoCommands.hpp"

namespace aur::editor
{

    // Resolves the region to a mutable Json array. Out-of-range or missing
    // regions return null so a stale command cannot crash a resized document.
    Json *CellCommand::targetArray(const RegionKey &region) const
    {
        if (!m_document || !m_document->is_object())
            return nullptr;
        switch (region.kind)
        {
        case RegionKey::MapLayer:
        {
            // The document is the map object; layers.<name> is rows-of-rows
            // and the cell is rows[row][col]. index picks the layer name.
            const char *layerName = region.index == 0 ? "ground"
                                    : region.index == 1 ? "objects"
                                                        : "above";
            if (!m_document->contains("layers"))
                return nullptr;
            Json &layers = (*m_document)["layers"];
            if (!layers.is_object() || !layers.contains(layerName))
                return nullptr;
            Json &layer = layers[layerName];
            if (!layer.is_array() || region.row >= static_cast<int>(layer.as_array().size()))
                return nullptr;
            Json &row = layer[static_cast<size_t>(region.row)];
            return row.is_array() ? &row : nullptr;
        }
        case RegionKey::MapEntities:
            return m_document->contains("entities") && (*m_document)["entities"].is_array()
                       ? &(*m_document)["entities"]
                       : nullptr;
        case RegionKey::GraphicPixels:
        {
            if (!m_document->contains("graphics") || !(*m_document)["graphics"].is_array() ||
                region.index >= static_cast<int>((*m_document)["graphics"].as_array().size()))
                return nullptr;
            Json &graphic = (*m_document)["graphics"][static_cast<size_t>(region.index)];
            return graphic.contains("pixels") && graphic["pixels"].is_array()
                       ? &graphic["pixels"]
                       : nullptr;
        }
        case RegionKey::TileFlags:
        {
            if (!m_document->contains("tiles") || !(*m_document)["tiles"].is_array() ||
                region.index >= static_cast<int>((*m_document)["tiles"].as_array().size()))
                return nullptr;
            Json &tile = (*m_document)["tiles"][static_cast<size_t>(region.index)];
            return tile.contains("flags") && tile["flags"].is_array()
                       ? &tile["flags"]
                       : nullptr;
        }
        default:
            return nullptr;
        }
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
        if (!target)
            return;
        for (const CellDiff &diff : m_diffs)
        {
            // MapLayer addresses a cell inside its resolved row; the others
            // index the resolved array directly.
            const int cell = m_region.kind == RegionKey::MapLayer ? m_region.col
                                                                  : diff.cellIndex;
            if (cell < 0 || cell >= static_cast<int>(target->as_array().size()))
                continue;
            target->as_array()[static_cast<size_t>(cell)] = after ? diff.after : diff.before;
        }
    }

} // namespace aur::editor
