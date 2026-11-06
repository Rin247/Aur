#include "mini_test.hpp"
#include "world/MapView.hpp"

void test_map_view_layers() {
    aur::Project project;
    project.maps["sample"] = aur::JsonObject{
        {"w", 2}, {"h", 2},
        {"layers", aur::JsonObject{
            {"elevation", aur::JsonArray{aur::JsonArray{11, 22}, aur::JsonArray{33, 44}}},
            {"biomes", aur::JsonArray{aur::JsonArray{"plains", "forest"},
                                       aur::JsonArray{"desert", "tundra"}}}
        }}
    };
    auto view = aur::world::MapView::current(&project, "sample");
    ASSERT(view.has_value());
    ASSERT(view->width() == 2);
    ASSERT(view->height() == 2);
    ASSERT(view->elevationAt(1, 0) == 22);
    ASSERT(view->elevationAt(-1, 0, -7) == -7);
    ASSERT(view->elevationAt(0, 2, -8) == -8);
    ASSERT(view->biomeAt(0, 1) == "desert");
    ASSERT(view->biomeAt(2, 1).empty());
    ASSERT(view->entities() == nullptr);
}
