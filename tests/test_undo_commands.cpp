#include "mini_test.hpp"
#include "map/MapEditUndo.hpp"
#include "core/UndoCommands.hpp"

#include <QUndoStack>

#include <iostream>

namespace
{
    using aur::Json;
    using aur::JsonArray;
    using aur::JsonObject;
    using aur::editor::CellCommand;
    using aur::editor::CellDiff;
    using aur::editor::MapEditUndo;
    using aur::editor::RegionKey;
    using aur::editor::SnapshotCommand;

    // Map-shaped fixture: layers.ground is rows-of-rows (3 rows of 4) and a
    // map's w/h are the grid size.
    Json mapDocument()
    {
        return JsonObject{
            {"w", 4}, {"h", 3},
            {"layers", JsonObject{
                {"ground", JsonArray{
                    Json(JsonArray{1, 1, 1, 1}),
                    Json(JsonArray{2, 2, 2, 2}),
                    Json(JsonArray{3, 3, 3, 3})}},
                {"objects", JsonArray{
                    Json(JsonArray{0, 0, 0, 0})}},
                {"above", JsonArray{
                    Json(JsonArray{7, 7, 7, 7})}}}}};
    }

    // Tileset-shaped fixture: tiles[i].flags is an int bitmask, and one cell
    // of the region is one whole tile object.
    Json tilesetDocument()
    {
        return JsonObject{
            {"tiles", JsonArray{
                JsonObject{{"name", "grass"}, {"flags", 0}},
                JsonObject{{"name", "water"}, {"flags", 1}},
                JsonObject{{"name", "rock"}, {"flags", 2}}}}};
    }

    // Entity-shaped fixture: the map's entities array holds whole objects.
    Json entityDocument()
    {
        return JsonObject{
            {"entities", JsonArray{
                JsonObject{{"id", "door_0"}, {"x", 3}, {"y", 4}},
                JsonObject{{"id", "door_1"}, {"x", 8}, {"y", 4}}}}};
    }

    // Pixel-shaped fixture: a frame is an array of row strings.
    Json pixelDocument()
    {
        JsonObject frames;
        frames["down_0"] = JsonArray{".xx.", "x..x"};
        JsonObject graphic;
        graphic["name"] = "Hero";
        graphic["frames"] = frames;
        JsonObject document;
        document["graphics"] = JsonArray{graphic};
        return document;
    }

    void test_map_layer_cell_apply_and_restore()
    {
        Json doc = mapDocument();
        CellCommand command(QStringLiteral("paint ground"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::MapLayer;
        region.index = 0; // ground
        command.setRegion(region);
        CellDiff diff;
        diff.before = Json(3);
        diff.after = Json(9);
        diff.row = 2;
        diff.col = 1;
        command.addDiff(diff);

        command.redo();
        ASSERT(doc["layers"]["ground"][2][1].as_int() == 9);
        // The whole layer resolved: neighbouring rows and columns untouched.
        ASSERT(doc["layers"]["ground"][1][1].as_int() == 2);
        ASSERT(doc["layers"]["ground"][2][0].as_int() == 3);
        ASSERT(doc["layers"]["ground"][0].as_array().size() == 4);

        command.undo();
        ASSERT(doc["layers"]["ground"][2][1].as_int() == 3);
        ASSERT(doc["layers"]["ground"].as_array().size() == 3);
    }

    void test_map_layer_other_layer_names_resolve()
    {
        Json doc = mapDocument();
        CellCommand objects(QStringLiteral("paint objects"));
        objects.setDocument(&doc);
        RegionKey objects_region;
        objects_region.kind = RegionKey::MapLayer;
        objects_region.index = 1; // objects
        objects.setRegion(objects_region);
        CellDiff objects_diff;
        objects_diff.before = Json(0);
        objects_diff.after = Json(5);
        objects_diff.row = 0;
        objects_diff.col = 2;
        objects.addDiff(objects_diff);
        objects.redo();
        ASSERT(doc["layers"]["objects"][0][2].as_int() == 5);
        ASSERT(doc["layers"]["ground"][0][2].as_int() == 1);

        CellCommand above(QStringLiteral("paint above"));
        above.setDocument(&doc);
        RegionKey above_region;
        above_region.kind = RegionKey::MapLayer;
        above_region.index = 2; // above
        above.setRegion(above_region);
        CellDiff above_diff;
        above_diff.before = Json(7);
        above_diff.after = Json(8);
        above_diff.row = 0;
        above_diff.col = 3;
        above.addDiff(above_diff);
        above.redo();
        ASSERT(doc["layers"]["above"][0][3].as_int() == 8);
    }

    void test_map_layer_unknown_layer_is_a_noop()
    {
        Json doc = mapDocument();
        CellCommand command(QStringLiteral("nonsense"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::MapLayer;
        region.index = 9; // no such layer
        command.setRegion(region);
        CellDiff diff;
        diff.before = Json(1);
        diff.after = Json(9);
        diff.row = 0;
        diff.col = 0;
        command.addDiff(diff);

        command.redo();
        command.undo();
        ASSERT(doc["layers"]["ground"][0][0].as_int() == 1);
    }

    void test_tile_object_cell_apply_and_restore()
    {
        Json doc = tilesetDocument();
        CellCommand command(QStringLiteral("toggle flag"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::TileObjects;
        region.index = 2; // rock
        command.setRegion(region);
        Json before = doc["tiles"][2];
        Json after = before;
        after["flags"] = 6;
        CellDiff diff;
        diff.cellIndex = 2;
        diff.before = before;
        diff.after = after;
        command.addDiff(diff);

        command.redo();
        ASSERT(doc["tiles"][2]["flags"].as_int() == 6);
        ASSERT(doc["tiles"][2]["name"].as_string() == "rock");
        // The other tiles keep their flags.
        ASSERT(doc["tiles"][1]["flags"].as_int() == 1);
        ASSERT(doc["tiles"][0]["flags"].as_int() == 0);

        command.undo();
        ASSERT(doc["tiles"][2]["flags"].as_int() == 2);
        ASSERT(doc["tiles"][2]["name"].as_string() == "rock");
    }

    void test_entity_cell_apply_and_restore()
    {
        Json doc = entityDocument();
        CellCommand command(QStringLiteral("move entity"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::MapEntities;
        command.setRegion(region);
        Json moved = JsonObject{{"id", "door_0"}, {"x", 6}, {"y", 7}};
        CellDiff diff;
        diff.cellIndex = 0;
        diff.before = doc["entities"][0];
        diff.after = moved;
        command.addDiff(diff);

        command.redo();
        ASSERT(doc["entities"][0]["x"].as_int() == 6);
        command.undo();
        ASSERT(doc["entities"][0]["x"].as_int() == 3);
        ASSERT(doc["entities"][0]["id"].as_string() == "door_0");
    }

    void test_direct_region_apply_and_restore()
    {
        Json doc = pixelDocument();
        Json &frame = doc["graphics"][0]["frames"]["down_0"];
        CellCommand command(QStringLiteral("paint pixels"));
        command.setDirect(&frame);
        RegionKey region;
        region.kind = RegionKey::Direct;
        command.setRegion(region);
        CellDiff diff;
        diff.cellIndex = 1;
        diff.before = frame.as_array()[1];
        diff.after = Json("xxx.");
        command.addDiff(diff);

        command.redo();
        ASSERT(frame.as_array()[1].as_string() == "xxx.");
        ASSERT(frame.as_array()[0].as_string() == ".xx.");
        command.undo();
        ASSERT(frame.as_array()[1].as_string() == "x..x");
    }

    void test_out_of_range_cell_is_safe()
    {
        Json doc = tilesetDocument();
        CellCommand command(QStringLiteral("nonsense"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::TileObjects;
        command.setRegion(region);
        CellDiff diff;
        diff.cellIndex = 99; // no such tile
        diff.before = JsonObject{{"name", "ghost"}, {"flags", 0}};
        diff.after = JsonObject{{"name", "ghost"}, {"flags", 1}};
        command.addDiff(diff);

        command.redo();
        command.undo();
        ASSERT(doc["tiles"].as_array().size() == 3);
    }

    void test_stale_command_over_resized_document_is_safe()
    {
        Json doc = tilesetDocument();
        CellCommand command(QStringLiteral("toggle stale flag"));
        command.setDocument(&doc);
        RegionKey stale_region;
        stale_region.kind = RegionKey::TileObjects;
        command.setRegion(stale_region);
        Json after = JsonObject{{"name", "rock"}, {"flags", 9}};
        CellDiff diff;
        diff.cellIndex = 2;
        diff.before = doc["tiles"][2];
        diff.after = after;
        command.addDiff(diff);

        // Shrink the array so the diff falls off the end.
        JsonArray shrunk;
        shrunk.emplace_back(doc["tiles"][0]);
        shrunk.emplace_back(doc["tiles"][1]);
        doc["tiles"] = shrunk;

        command.redo(); // must not crash or write out of range
        ASSERT(doc["tiles"].as_array().size() == 2);
    }

    void test_missing_region_is_a_noop()
    {
        Json doc = mapDocument();
        CellCommand command(QStringLiteral("nonsense"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::MapEntities;
        command.setRegion(region);
        CellDiff diff;
        diff.cellIndex = 0;
        diff.before = JsonObject{{"id", "ghost"}};
        diff.after = JsonObject{{"id", "phantom"}};
        command.addDiff(diff);

        command.redo();
        command.undo();
        ASSERT(doc.contains("entities") == false);
    }

    void test_snapshot_command_replaces_the_region()
    {
        Json doc = entityDocument();
        SnapshotCommand command(QStringLiteral("delete entity"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::MapEntities;
        command.setRegion(region);
        JsonArray one;
        one.emplace_back(doc["entities"][0]);
        command.setBefore(doc["entities"]);
        command.setAfter(one);

        command.redo();
        ASSERT(doc["entities"].as_array().size() == 1);
        command.undo();
        ASSERT(doc["entities"].as_array().size() == 2);
    }

    void test_map_stroke_records_one_command_with_per_cell_diffs()
    {
        Json doc = mapDocument();
        QUndoStack stack;
        MapEditUndo recorder;
        recorder.beginLayer(0, QStringLiteral("paint tiles"));
        Json &layer = doc["layers"]["ground"];
        recorder.noteCell(layer, 0, 0);
        layer.as_array()[0].as_array()[0] = 5;
        recorder.noteCell(layer, 1, 2);
        layer.as_array()[1].as_array()[2] = 6;
        // A cell painted twice keeps the value it had before the stroke.
        recorder.noteCell(layer, 1, 2);
        layer.as_array()[1].as_array()[2] = 7;
        ASSERT(recorder.commit(&doc, &stack));
        ASSERT(stack.count() == 1);
        ASSERT(stack.undoText() == "paint tiles");

        stack.undo();
        ASSERT(layer.as_array()[0].as_array()[0].as_int() == 1);
        ASSERT(layer.as_array()[1].as_array()[2].as_int() == 2);

        stack.redo();
        ASSERT(layer.as_array()[0].as_array()[0].as_int() == 5);
        ASSERT(layer.as_array()[1].as_array()[2].as_int() == 7);
    }

    void test_map_stroke_without_a_change_pushes_nothing()
    {
        Json doc = mapDocument();
        QUndoStack stack;
        MapEditUndo recorder;
        recorder.beginLayer(0, QStringLiteral("noop stroke"));
        Json &layer = doc["layers"]["ground"];
        recorder.noteCell(layer, 0, 0);
        ASSERT(!recorder.commit(&doc, &stack));
        ASSERT(stack.count() == 0);
    }

    void test_map_entity_stroke_snapshots_the_array()
    {
        Json doc = entityDocument();
        QUndoStack stack;
        MapEditUndo recorder;
        recorder.beginEntities(doc["entities"], QStringLiteral("place entity"));
        Json placed = JsonObject{{"id", "chest_1"}, {"type", "chest"}, {"x", 2}, {"y", 2}};
        doc["entities"].as_array().push_back(placed);
        ASSERT(recorder.commit(&doc, &stack));
        ASSERT(stack.count() == 1);
        ASSERT(doc["entities"].as_array().size() == 3);

        stack.undo();
        ASSERT(doc["entities"].as_array().size() == 2);
        stack.redo();
        ASSERT(doc["entities"].as_array().size() == 3);
        ASSERT(doc["entities"][2]["id"].as_string() == "chest_1");
    }
} // namespace

void test_undo_commands()
{
    test_map_layer_cell_apply_and_restore();
    test_map_layer_other_layer_names_resolve();
    test_map_layer_unknown_layer_is_a_noop();
    test_tile_object_cell_apply_and_restore();
    test_entity_cell_apply_and_restore();
    test_direct_region_apply_and_restore();
    test_out_of_range_cell_is_safe();
    test_stale_command_over_resized_document_is_safe();
    test_missing_region_is_a_noop();
    test_snapshot_command_replaces_the_region();
    test_map_stroke_records_one_command_with_per_cell_diffs();
    test_map_stroke_without_a_change_pushes_nothing();
    test_map_entity_stroke_snapshots_the_array();
}
