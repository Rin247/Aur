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
            // layers.{ground,objects,above}[index] is the row array; slot
            // distinguishes the layer name, index the row.
            return nullptr; // wired by the map editor in D4.S1b
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

    void CellCommand::apply(bool after)
    {
        Json *target = targetArray(m_region);
        if (!target)
            return;
        for (const CellDiff &diff : m_diffs)
        {
            const int cell = diff.cellIndex;
            if (cell < 0 || cell >= static_cast<int>(target->as_array().size()))
                continue;
            target->as_array()[static_cast<size_t>(cell)] = after ? diff.after : diff.before;
        }
    }

} // namespace aur::editor
