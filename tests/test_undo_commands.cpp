#include "mini_test.hpp"
#include "UndoCommands.hpp"

#include <QCoreApplication>

namespace
{
    using aur::Json;
    using aur::JsonArray;
    using aur::JsonObject;
    using aur::editor::CellCommand;
    using aur::editor::CellDiff;
    using aur::editor::RegionKey;

    // Pixel-shaped fixture: graphics[0].pixels with 8 cells.
    Json pixelDocument()
    {
        return JsonObject{
            {"graphics", JsonArray{JsonObject{
                {"name", "Hero"},
                {"pixels", JsonArray{0, 1, 2, 3, 4, 5, 6, 7}}}}}};
    }

    // Tileset-shaped fixture: tiles[2].flags with 4 cells.
    Json tilesetDocument()
    {
        return JsonObject{
            {"tiles", JsonArray{
                JsonObject{{"name", "grass"}, {"flags", JsonArray{0, 0}}},
                JsonObject{{"name", "water"}, {"flags", JsonArray{1, 0}}},
                JsonObject{{"name", "rock"}, {"flags", JsonArray{0, 1, 1, 0}}}}}};
    }

    // Entity-shaped fixture: top-level entities array of objects.
    Json entityDocument()
    {
        return JsonObject{
            {"entities", JsonArray{
                JsonObject{{"id", "door_0"}, {"x", 3}, {"y", 4}},
                JsonObject{{"id", "door_1"}, {"x", 8}, {"y", 4}}}}};
    }

    void test_pixel_cell_apply_and_restore()
    {
        Json doc = pixelDocument();
        auto *pixels = &doc["graphics"][0]["pixels"];

        CellCommand command(QStringLiteral("paint pixels"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::GraphicPixels;
        region.index = 0;
        command.setRegion(region);
        command.addDiff(CellDiff{2, Json(2), Json(9)});
        command.addDiff(CellDiff{5, Json(5), Json(9)});

        command.redo();
        ASSERT(pixels->as_array()[2].as_int() == 9);
        ASSERT(pixels->as_array()[5].as_int() == 9);
        // Untouched neighbours keep their values.
        ASSERT(pixels->as_array()[3].as_int() == 3);

        command.undo();
        ASSERT(pixels->as_array()[2].as_int() == 2);
        ASSERT(pixels->as_array()[5].as_int() == 5);
    }

    void test_flag_cell_apply_and_restore()
    {
        Json doc = tilesetDocument();
        CellCommand command(QStringLiteral("toggle flags"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::TileFlags;
        region.index = 2;
        command.setRegion(region);
        command.addDiff(CellDiff{0, Json(0), Json(1)});

        command.redo();
        ASSERT(doc["tiles"][2]["flags"][0].as_int() == 1);
        // Other tiles' flags are untouched.
        ASSERT(doc["tiles"][1]["flags"][0].as_int() == 1);

        command.undo();
        ASSERT(doc["tiles"][2]["flags"][0].as_int() == 0);
    }

    void test_entity_row_apply_and_restore()
    {
        Json doc = entityDocument();
        CellCommand command(QStringLiteral("move entity"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::MapEntities;
        command.setRegion(region);
        Json moved = JsonObject{{"id", "door_0"}, {"x", 6}, {"y", 7}};
        command.addDiff(CellDiff{0, doc["entities"][0], moved});

        command.redo();
        ASSERT(doc["entities"][0]["x"].as_int() == 6);
        command.undo();
        ASSERT(doc["entities"][0]["x"].as_int() == 3);
        ASSERT(doc["entities"][0]["id"].as_string() == "door_0");
    }

    void test_stale_command_over_resized_document_is_safe()
    {
        Json doc = pixelDocument();
        CellCommand command(QStringLiteral("paint"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::GraphicPixels;
        region.index = 0;
        command.setRegion(region);
        command.addDiff(CellDiff{5, Json(5), Json(9)});
        command.addDiff(CellDiff{100, Json(0), Json(9)}); // out of range after resize

        // Shrink the array so the second diff falls off the end.
        JsonArray shrunk;
        for (int i = 0; i < 6; ++i)
            shrunk.emplace_back(i);
        doc["graphics"][0]["pixels"] = shrunk;

        command.redo(); // must not crash or write out of range
        ASSERT(doc["graphics"][0]["pixels"].as_array().size() == 6);
        ASSERT(doc["graphics"][0]["pixels"][5].as_int() == 9);
    }

    void test_missing_region_is_a_noop()
    {
        Json doc = pixelDocument();
        CellCommand command(QStringLiteral("nonsense"));
        command.setDocument(&doc);
        RegionKey region;
        region.kind = RegionKey::GraphicPixels;
        region.index = 9; // no such frame
        command.setRegion(region);
        command.addDiff(CellDiff{0, Json(0), Json(9)});

        command.redo();
        command.undo();
        ASSERT(doc["graphics"][0]["pixels"][0].as_int() == 0);
    }
} // namespace

void test_undo_commands()
{
    test_pixel_cell_apply_and_restore();
    test_flag_cell_apply_and_restore();
    test_entity_row_apply_and_restore();
    test_stale_command_over_resized_document_is_safe();
    test_missing_region_is_a_noop();
}
