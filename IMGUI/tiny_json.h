#ifndef TINY_JSON_H
#define TINY_JSON_H

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

// A deliberately small, read-only JSON decoder. It understands the complete
// JSON value grammar but exposes only the field types used by the attendance
// API. Parsing is bounded by the HTTP response limit in APIClient.
class TinyJson {
public:
    struct ObjectRange {
        size_t begin;
        size_t end;
        bool valid;

        ObjectRange() : begin(0), end(0), valid(false) {}
    };

    explicit TinyJson(std::string source) : source_(std::move(source)) {}

    bool RootObject(ObjectRange& out) const {
        size_t pos = 0;
        SkipWhitespace(pos);
        if (pos >= source_.size() || source_[pos] != '{') return false;
        size_t end = pos;
        if (!SkipValue(end, 0)) return false;
        size_t trailing = end;
        SkipWhitespace(trailing);
        if (trailing != source_.size()) return false;
        out.begin = pos;
        out.end = end;
        out.valid = true;
        return true;
    }

    bool GetString(const ObjectRange& object, const std::string& key, std::string& out) const {
        size_t value = 0;
        if (!FindMember(object, key, value) || value >= source_.size() || source_[value] != '"') return false;
        return ParseString(value, out);
    }

    bool GetBool(const ObjectRange& object, const std::string& key, bool& out) const {
        size_t value = 0;
        if (!FindMember(object, key, value)) return false;
        if (source_.compare(value, 4, "true") == 0) {
            out = true;
            return true;
        }
        if (source_.compare(value, 5, "false") == 0) {
            out = false;
            return true;
        }
        return false;
    }

    bool GetObject(const ObjectRange& object, const std::string& key, ObjectRange& out) const {
        size_t value = 0;
        if (!FindMember(object, key, value) || value >= source_.size() || source_[value] != '{') return false;
        size_t end = value;
        if (!SkipValue(end, 0)) return false;
        out.begin = value;
        out.end = end;
        out.valid = true;
        return true;
    }

    bool GetStringArray(const ObjectRange& object,
                        const std::string& key,
                        std::vector<std::string>& out) const {
        size_t pos = 0;
        if (!FindMember(object, key, pos) || pos >= source_.size() || source_[pos] != '[') return false;
        ++pos;
        SkipWhitespace(pos);
        out.clear();
        if (pos < source_.size() && source_[pos] == ']') return true;

        while (pos < source_.size()) {
            std::string value;
            if (source_[pos] != '"' || !ParseString(pos, value)) return false;
            out.push_back(value);
            SkipWhitespace(pos);
            if (pos >= source_.size()) return false;
            if (source_[pos] == ']') return true;
            if (source_[pos] != ',') return false;
            ++pos;
            SkipWhitespace(pos);
        }
        return false;
    }

private:
    std::string source_;

    void SkipWhitespace(size_t& pos) const {
        while (pos < source_.size()) {
            const char c = source_[pos];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            ++pos;
        }
    }

    static bool IsHex(char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    static uint32_t HexValue(char c) {
        if (c >= '0' && c <= '9') return static_cast<uint32_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint32_t>(10 + c - 'a');
        return static_cast<uint32_t>(10 + c - 'A');
    }

    static void AppendUtf8(uint32_t codepoint, std::string& out) {
        if (codepoint <= 0x7f) {
            out.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            out.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            out.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    bool ParseUnicode(size_t& pos, uint32_t& codepoint) const {
        if (pos + 4 > source_.size()) return false;
        codepoint = 0;
        for (int i = 0; i < 4; ++i) {
            if (!IsHex(source_[pos + i])) return false;
            codepoint = (codepoint << 4) | HexValue(source_[pos + i]);
        }
        pos += 4;
        return true;
    }

    bool ParseString(size_t& pos, std::string& out) const {
        if (pos >= source_.size() || source_[pos] != '"') return false;
        ++pos;
        out.clear();
        while (pos < source_.size()) {
            const unsigned char c = static_cast<unsigned char>(source_[pos++]);
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') {
                out.push_back(static_cast<char>(c));
                continue;
            }
            if (pos >= source_.size()) return false;
            const char escaped = source_[pos++];
            switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    uint32_t first = 0;
                    if (!ParseUnicode(pos, first)) return false;
                    if (first >= 0xd800 && first <= 0xdbff) {
                        if (pos + 2 > source_.size() || source_[pos] != '\\' || source_[pos + 1] != 'u') return false;
                        pos += 2;
                        uint32_t second = 0;
                        if (!ParseUnicode(pos, second) || second < 0xdc00 || second > 0xdfff) return false;
                        first = 0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00);
                    } else if (first >= 0xdc00 && first <= 0xdfff) {
                        return false;
                    }
                    AppendUtf8(first, out);
                    break;
                }
                default: return false;
            }
        }
        return false;
    }

    bool SkipNumber(size_t& pos) const {
        const size_t start = pos;
        if (pos < source_.size() && source_[pos] == '-') ++pos;
        if (pos >= source_.size()) return false;
        if (source_[pos] == '0') {
            ++pos;
        } else {
            if (source_[pos] < '1' || source_[pos] > '9') return false;
            while (pos < source_.size() && source_[pos] >= '0' && source_[pos] <= '9') ++pos;
        }
        if (pos < source_.size() && source_[pos] == '.') {
            ++pos;
            const size_t fraction = pos;
            while (pos < source_.size() && source_[pos] >= '0' && source_[pos] <= '9') ++pos;
            if (pos == fraction) return false;
        }
        if (pos < source_.size() && (source_[pos] == 'e' || source_[pos] == 'E')) {
            ++pos;
            if (pos < source_.size() && (source_[pos] == '+' || source_[pos] == '-')) ++pos;
            const size_t exponent = pos;
            while (pos < source_.size() && source_[pos] >= '0' && source_[pos] <= '9') ++pos;
            if (pos == exponent) return false;
        }
        return pos > start;
    }

    bool SkipValue(size_t& pos, int depth) const {
        if (depth > 32) return false;
        SkipWhitespace(pos);
        if (pos >= source_.size()) return false;
        if (source_[pos] == '"') {
            std::string ignored;
            return ParseString(pos, ignored);
        }
        if (source_[pos] == '{') {
            ++pos;
            SkipWhitespace(pos);
            if (pos < source_.size() && source_[pos] == '}') {
                ++pos;
                return true;
            }
            while (pos < source_.size()) {
                std::string ignored;
                if (!ParseString(pos, ignored)) return false;
                SkipWhitespace(pos);
                if (pos >= source_.size() || source_[pos++] != ':') return false;
                if (!SkipValue(pos, depth + 1)) return false;
                SkipWhitespace(pos);
                if (pos >= source_.size()) return false;
                if (source_[pos] == '}') {
                    ++pos;
                    return true;
                }
                if (source_[pos++] != ',') return false;
                SkipWhitespace(pos);
            }
            return false;
        }
        if (source_[pos] == '[') {
            ++pos;
            SkipWhitespace(pos);
            if (pos < source_.size() && source_[pos] == ']') {
                ++pos;
                return true;
            }
            while (pos < source_.size()) {
                if (!SkipValue(pos, depth + 1)) return false;
                SkipWhitespace(pos);
                if (pos >= source_.size()) return false;
                if (source_[pos] == ']') {
                    ++pos;
                    return true;
                }
                if (source_[pos++] != ',') return false;
                SkipWhitespace(pos);
            }
            return false;
        }
        if (source_.compare(pos, 4, "true") == 0 || source_.compare(pos, 4, "null") == 0) {
            pos += 4;
            return true;
        }
        if (source_.compare(pos, 5, "false") == 0) {
            pos += 5;
            return true;
        }
        return SkipNumber(pos);
    }

    bool FindMember(const ObjectRange& object, const std::string& key, size_t& value_pos) const {
        if (!object.valid || object.begin >= source_.size() || source_[object.begin] != '{') return false;
        size_t pos = object.begin + 1;
        SkipWhitespace(pos);
        if (pos < object.end && source_[pos] == '}') return false;
        while (pos < object.end) {
            std::string member_name;
            if (!ParseString(pos, member_name)) return false;
            SkipWhitespace(pos);
            if (pos >= object.end || source_[pos++] != ':') return false;
            SkipWhitespace(pos);
            if (member_name == key) {
                value_pos = pos;
                return true;
            }
            if (!SkipValue(pos, 1)) return false;
            SkipWhitespace(pos);
            if (pos >= object.end || source_[pos] == '}') return false;
            if (source_[pos++] != ',') return false;
            SkipWhitespace(pos);
        }
        return false;
    }
};

#endif
