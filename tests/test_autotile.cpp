#include "mini_test.hpp"
#include "aur/autotile.hpp"
#include "aur/json.hpp"
#include "aur/world_topology.hpp"

void test_autotile_basic() {
    aur::Json ts;
    aur::JsonArray tiles;
    for (int i = 0; i < 16; ++i) tiles.push_back(i);
    aur::JsonObject auto_group;
    auto_group["tiles"] = tiles;
    auto_group["edge_joined"] = true;
    aur::JsonObject autotiles;
    autotiles["path"] = auto_group;
    ts["autotiles"] = autotiles;

    aur::Json m;
    m["w"] = 3;
    m["h"] = 3;
    aur::JsonArray row;
    for (int x = 0; x < 3; ++x) row.push_back(-1);
    aur::JsonArray rows;
    rows.push_back(row); rows.push_back(row); rows.push_back(row);
    aur::JsonObject layers;
    layers["ground"] = rows;
    m["layers"] = layers;

    aur::paint(m, ts, "ground", 1, 1, "path");
    // Isolated patch with no matching neighbors -> mask 0 -> tile index 0
    ASSERT(m["layers"]["ground"][1].as_array()[1].as_int() == 0);

    m["worldgen"] = aur::JsonObject{{"topology", "continents"}};
    m["layers"]["ground"][1][1] = -1;
    m["layers"]["ground"][1][0] = 0;
    m["layers"]["ground"][1][2] = 0;
    ASSERT(aur::mask_at(m, ts, "ground", 0, 1, "path") == aur::W);
    ASSERT(aur::mask_at(m, ts, "ground", 2, 1, "path") == aur::E);
    m["layers"]["ground"][1][1] = 0;
    aur::paint(m, ts, "ground", -1, 1, "path");
    ASSERT(m["layers"]["ground"][1][2].as_int() != -1);
    ASSERT(aur::mask_at(m, ts, "ground", 2, 1, "path") == (aur::E | aur::W));

    ts["autotiles"]["path"]["edge_joined"] = true;
    aur::Json all_edges;
    all_edges["w"] = 3;
    all_edges["h"] = 3;
    all_edges["worldgen"] = aur::JsonObject{{"topology", "supercontinent"}};
    all_edges["layers"]["ground"] = aur::JsonArray{
        aur::JsonArray{-1, 0, -1}, aur::JsonArray{0, -1, 0}, aur::JsonArray{-1, 0, -1}};
    ASSERT(aur::mask_at(all_edges, ts, "ground", 0, 1, "path") == aur::W);
    ASSERT(aur::mask_at(all_edges, ts, "ground", 2, 1, "path") == aur::E);
    ASSERT(aur::mask_at(all_edges, ts, "ground", 1, 0, "path") == aur::N);
    ASSERT(aur::mask_at(all_edges, ts, "ground", 1, 2, "path") == aur::S);
    aur::paint(all_edges, ts, "ground", -1, 1, "path");
    ASSERT(aur::mask_at(all_edges, ts, "ground", 0, 1, "path") == aur::W);
    ASSERT(aur::mask_at(all_edges, ts, "ground", 2, 1, "path") == aur::E);
    aur::retile_all(all_edges, ts, "ground");
    ASSERT(aur::mask_at(all_edges, ts, "ground", 0, 1, "path") == aur::W);
    ASSERT(aur::mask_at(all_edges, ts, "ground", 2, 1, "path") == aur::E);
}
