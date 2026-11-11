#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <iomanip>

namespace aur {

enum class JsonType { Null, Bool, Number, String, Array, Object };

struct Json;

using JsonArray = std::vector<Json>;
using JsonObject = std::map<std::string, Json, std::less<>>; // ordered via std::map

struct Json {
    JsonType type = JsonType::Null;
    bool b_val = false;
    double n_val = 0.0;
    std::string s_val;
    JsonArray a_val;
    JsonObject o_val;

    Json() = default;
    Json(nullptr_t) : type(JsonType::Null) {}
    Json(bool v) : type(JsonType::Bool), b_val(v) {}
    Json(int v) : type(JsonType::Number), n_val(v) {}
    Json(long v) : type(JsonType::Number), n_val(v) {}
    Json(double v) : type(JsonType::Number), n_val(v) {}
    Json(const char* v) : type(JsonType::String), s_val(v ? v : "") {}
    Json(const std::string& v) : type(JsonType::String), s_val(v) {}
    Json(std::string&& v) : type(JsonType::String), s_val(std::move(v)) {}
    Json(const JsonArray& v) : type(JsonType::Array), a_val(v) {}
    Json(JsonArray&& v) : type(JsonType::Array), a_val(std::move(v)) {}
    Json(const JsonObject& v) : type(JsonType::Object), o_val(v) {}
    Json(JsonObject&& v) : type(JsonType::Object), o_val(std::move(v)) {}

    bool is_null() const { return type == JsonType::Null; }
    bool is_bool() const { return type == JsonType::Bool; }
    bool is_number() const { return type == JsonType::Number; }
    bool is_string() const { return type == JsonType::String; }
    bool is_array() const { return type == JsonType::Array; }
    bool is_object() const { return type == JsonType::Object; }

    bool as_bool() const { return b_val; }
    double as_number() const { return n_val; }
    int as_int() const { return static_cast<int>(n_val); }
    const std::string& as_string() const { return s_val; }
    const JsonArray& as_array() const { return a_val; }
    JsonArray& as_array() { return a_val; }
    const JsonObject& as_object() const { return o_val; }
    JsonObject& as_object() { return o_val; }

    const Json& operator[](const std::string& k) const {
        auto it = o_val.find(k);
        if (it != o_val.end()) return it->second;
        static Json null_json;
        return null_json;
    }
    Json& operator[](const std::string& k) {
        type = JsonType::Object;
        return o_val[k];
    }

    const Json& operator[](size_t i) const {
        return a_val.at(i);
    }
    Json& operator[](size_t i) {
        type = JsonType::Array;
        return a_val.at(i);
    }

    bool contains(const std::string& k) const {
        return type == JsonType::Object && o_val.find(k) != o_val.end();
    }

    bool operator==(const Json& other) const {
        if (type != other.type) return false;
        switch (type) {
        case JsonType::Null: return true;
        case JsonType::Bool: return b_val == other.b_val;
        case JsonType::Number: return n_val == other.n_val;
        case JsonType::String: return s_val == other.s_val;
        case JsonType::Array: return a_val == other.a_val;
        case JsonType::Object: return o_val == other.o_val;
        }
        return false;
    }
};

class JsonParser {
    std::string src;
    size_t idx = 0;

    void skip_ws() {
        while (idx < src.size() && (std::isspace(src[idx]) || src[idx] == '\r' || src[idx] == '\n' || src[idx] == '\t')) {
            idx++;
        }
    }

    char peek() { skip_ws(); return idx < src.size() ? src[idx] : '\0'; }
    char get() { skip_ws(); return idx < src.size() ? src[idx++] : '\0'; }

    std::string parse_string() {
        get(); // consume opening quote
        std::string s;
        while (idx < src.size()) {
            char c = src[idx++];
            if (c == '"') return s;
            if (c == '\\' && idx < src.size()) {
                char esc = src[idx++];
                switch (esc) {
                    case '"': s.push_back('"'); break;
                    case '\\': s.push_back('\\'); break;
                    case '/': s.push_back('/'); break;
                    case 'b': s.push_back('\b'); break;
                    case 'f': s.push_back('\f'); break;
                    case 'n': s.push_back('\n'); break;
                    case 'r': s.push_back('\r'); break;
                    case 't': s.push_back('\t'); break;
                    default: s.push_back(esc); break;
                }
            } else {
                s.push_back(c);
            }
        }
        throw std::runtime_error("Unterminated string in JSON");
    }

    Json parse_number() {
        size_t start = idx;
        if (src[idx] == '-') idx++;
        while (idx < src.size() && (std::isdigit(src[idx]) || src[idx] == '.' || src[idx] == 'e' || src[idx] == 'E' || src[idx] == '+' || src[idx] == '-')) {
            idx++;
        }
        std::string num_str = src.substr(start, idx - start);
        return Json(std::stod(num_str));
    }

    Json parse_value() {
        char c = peek();
        if (c == '"') return Json(parse_string());
        if (c == '{') return Json(parse_object());
        if (c == '[') return Json(parse_array());
        if (c == 't' || c == 'f') {
            if (src.compare(idx, 4, "true") == 0) { idx += 4; return Json(true); }
            if (src.compare(idx, 5, "false") == 0) { idx += 5; return Json(false); }
        }
        if (c == 'n') {
            if (src.compare(idx, 4, "null") == 0) { idx += 4; return Json(nullptr); }
        }
        if (c == '-' || std::isdigit(c)) {
            return parse_number();
        }
        throw std::runtime_error(std::string("Unexpected character in JSON: ") + c);
    }

    JsonArray parse_array() {
        get(); // consume '['
        JsonArray arr;
        skip_ws();
        if (peek() == ']') { get(); return arr; }
        while (true) {
            arr.push_back(parse_value());
            skip_ws();
            char c = get();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("Expected ',' or ']' in JSON array");
            skip_ws();
        }
        return arr;
    }

JsonObject parse_object() {
        get(); // consume '{'
        JsonObject obj;
        skip_ws();
        if (peek() == '}') { get(); return obj; }
        while (true) {
            skip_ws();
            if (peek() != '"') throw std::runtime_error("Expected string key in JSON object");
            std::string key = parse_string();
            skip_ws();
            if (get() != ':') throw std::runtime_error("Expected ':' after key in JSON object");
            skip_ws();
            obj[key] = parse_value();
            skip_ws();
            char c = get();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("Expected ',' or '}' in JSON object");
            skip_ws();
        }
        return obj;
    }

public:
    explicit JsonParser(std::string s) : src(std::move(s)) {}
    Json parse() {
        skip_ws();
        Json val = parse_value();
        skip_ws();
        return val;
    }
};

inline Json parse_json(const std::string& s) {
    return JsonParser(s).parse();
}

inline std::string dump_json(const Json& j, int indent = 1, int level = 0) {
    std::ostringstream ss;
    std::string ind(level * indent, ' ');
    std::string next_ind((level + 1) * indent, ' ');

    switch (j.type) {
        case JsonType::Null:
            return "null";
        case JsonType::Bool:
            return j.b_val ? "true" : "false";
        case JsonType::Number: {
            double d = j.n_val;
            if (d == static_cast<long long>(d)) {
                ss << static_cast<long long>(d);
            } else {
                ss << std::setprecision(6) << d;
            }
            return ss.str();
        }
        case JsonType::String: {
            ss << '"';
            for (char c : j.s_val) {
                switch (c) {
                    case '"': ss << "\\\""; break;
                    case '\\': ss << "\\\\"; break;
                    case '\b': ss << "\\b"; break;
                    case '\f': ss << "\\f"; break;
                    case '\n': ss << "\\n"; break;
                    case '\r': ss << "\\r"; break;
                    case '\t': ss << "\\t"; break;
                    default: ss << c; break;
                }
            }
            ss << '"';
            return ss.str();
        }
        case JsonType::Array: {
            if (j.a_val.empty()) return "[]";
            // Check if it's a flat array of numbers/ints (compact numeric array formatting like Python version)
            bool all_nums = true;
            for (const auto& el : j.a_val) {
                if (!el.is_number()) { all_nums = false; break; }
            }
            if (all_nums && j.a_val.size() <= 32) {
                ss << "[";
                for (size_t i = 0; i < j.a_val.size(); ++i) {
                    if (i > 0) ss << ", ";
                    ss << dump_json(j.a_val[i], indent, level + 1);
                }
                ss << "]";
                return ss.str();
            }
            ss << "[\n";
            for (size_t i = 0; i < j.a_val.size(); ++i) {
                ss << next_ind << dump_json(j.a_val[i], indent, level + 1);
                if (i + 1 < j.a_val.size()) ss << ",";
                ss << "\n";
            }
            ss << ind << "]";
            return ss.str();
        }
        case JsonType::Object: {
            if (j.o_val.empty()) return "{}";
            ss << "{\n";
            size_t i = 0;
            for (auto const& [k, v] : j.o_val) {
                ss << next_ind << "\"" << k << "\": " << dump_json(v, indent, level + 1);
                if (++i < j.o_val.size()) ss << ",";
                ss << "\n";
            }
            ss << ind << "}";
            return ss.str();
        }
    }
    return "null";
}

} // namespace aur
