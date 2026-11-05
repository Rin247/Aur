#include "mini_test.hpp"
#include "aur/json.hpp"

void test_json_basic() {
    std::string s = R"({"title": "Aur", "val": 42, "flag": true, "arr": [1, 2, 3]})";
    auto j = aur::parse_json(s);
    ASSERT(j["title"].as_string() == "Aur");
    ASSERT(j["val"].as_int() == 42);
    ASSERT(j["flag"].as_bool() == true);
    ASSERT(j["arr"].as_array().size() == 3);
    ASSERT(j["arr"][0].as_int() == 1);
}

void test_json_roundtrip() {
    std::string s = R"({"a": 10, "b": "hello"})";
    auto j = aur::parse_json(s);
    std::string dumped = aur::dump_json(j, 1, 0);
    auto j2 = aur::parse_json(dumped);
    ASSERT(j2["a"].as_int() == 10);
    ASSERT(j2["b"].as_string() == "hello");
}
