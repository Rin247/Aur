#include "MapEditUndo.hpp"

#include <memory>

namespace aur::editor
{

    void MapEditUndo::beginLayer(int layer_index, const QString &label)
    {
        if (m_open)
            return;
        m_region = RegionKey{};
        m_region.kind = RegionKey::MapLayer;
        m_region.index = layer_index;
        m_label = label;
        m_before.clear();
        m_entities = false;
        m_open = true;
    }

    void MapEditUndo::beginEntities(Json &entities, const QString &label)
    {
        if (m_open)
            return;
        m_region = RegionKey{};
        m_region.kind = RegionKey::MapEntities;
        m_label = label;
        m_entities_before = entities;
        m_before.clear();
        m_entities = true;
        m_open = true;
    }

    void MapEditUndo::noteCell(Json &layer, int row, int col)
    {
        if (!m_open || m_entities || row < 0 || col < 0)
            return;
        JsonArray &rows = layer.as_array();
        if (row >= static_cast<int>(rows.size()))
            return;
        Json &cells = rows[static_cast<size_t>(row)];
        if (!cells.is_array() || col >= static_cast<int>(cells.as_array().size()))
            return;
        m_before.emplace(std::make_pair(row, col), cells.as_array()[static_cast<size_t>(col)]);
    }

    bool MapEditUndo::commit(Json *document, QUndoStack *stack)
    {
        if (!m_open)
            return false;
        m_open = false;
        if (!document || !stack)
            return false;
        Json *target = resolveRegion(m_region, document, nullptr);
        if (!target || !target->is_array())
            return false;
        if (m_entities)
        {
            if (*target == m_entities_before)
                return false;
            auto command = std::make_unique<SnapshotCommand>(m_label);
            command->setRegion(m_region);
            command->setDocument(document);
            command->setBefore(m_entities_before);
            command->setAfter(*target);
            stack->push(command.release());
            return true;
        }
        auto command = std::make_unique<CellCommand>(m_label);
        command->setRegion(m_region);
        command->setDocument(document);
        int changes = 0;
        JsonArray &rows = target->as_array();
        for (const auto &[cell, before] : m_before)
        {
            if (cell.first >= static_cast<int>(rows.size()))
                continue;
            Json &row = rows[static_cast<size_t>(cell.first)];
            if (!row.is_array() || cell.second >= static_cast<int>(row.as_array().size()))
                continue;
            const Json &after = row.as_array()[static_cast<size_t>(cell.second)];
            if (after == before)
                continue;
            CellDiff diff;
            diff.before = before;
            diff.after = after;
            diff.row = cell.first;
            diff.col = cell.second;
            command->addDiff(diff);
            ++changes;
        }
        if (!changes)
            return false;
        stack->push(command.release());
        return true;
    }

    void MapEditUndo::cancel()
    {
        m_open = false;
        m_before.clear();
    }

} // namespace aur::editor
