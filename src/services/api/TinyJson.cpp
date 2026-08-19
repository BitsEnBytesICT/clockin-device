#include "services/api/TinyJson.hpp"

#include <utility>

namespace clockin {

TinyJson::TinyJson(std::string source) : source_(std::move(source)) {}

bool TinyJson::RootObject(ObjectRange& output) const {
    std::size_t position = 0;
    SkipWhitespace(position);
    if (position >= source_.size() || source_[position] != '{') return false;
    std::size_t end = position;
    if (!SkipValue(end, 0)) return false;
    std::size_t trailing = end;
    SkipWhitespace(trailing);
    if (trailing != source_.size()) return false;
    output.begin = position;
    output.end = end;
    output.valid = true;
    return true;
}

bool TinyJson::GetString(const ObjectRange& object,
                         const std::string& key,
                         std::string& output) const {
    std::size_t value = 0;
    if (!FindMember(object, key, value) || value >= source_.size() || source_[value] != '"') {
        return false;
    }
    return ParseString(value, output);
}

bool TinyJson::GetBool(const ObjectRange& object, const std::string& key, bool& output) const {
    std::size_t value = 0;
    if (!FindMember(object, key, value)) return false;
    if (source_.compare(value, 4, "true") == 0) {
        output = true;
        return true;
    }
    if (source_.compare(value, 5, "false") == 0) {
        output = false;
        return true;
    }
    return false;
}

bool TinyJson::GetObject(const ObjectRange& object,
                         const std::string& key,
                         ObjectRange& output) const {
    std::size_t value = 0;
    if (!FindMember(object, key, value) || value >= source_.size() || source_[value] != '{') {
        return false;
    }
    std::size_t end = value;
    if (!SkipValue(end, 0)) return false;
    output.begin = value;
    output.end = end;
    output.valid = true;
    return true;
}

bool TinyJson::GetStringArray(const ObjectRange& object,
                              const std::string& key,
                              std::vector<std::string>& output) const {
    std::size_t position = 0;
    if (!FindMember(object, key, position) || position >= source_.size() ||
        source_[position] != '[') {
        return false;
    }
    ++position;
    SkipWhitespace(position);
    output.clear();
    if (position < source_.size() && source_[position] == ']') return true;
    while (position < source_.size()) {
        std::string value;
        if (source_[position] != '"' || !ParseString(position, value)) return false;
        output.push_back(value);
        SkipWhitespace(position);
        if (position >= source_.size()) return false;
        if (source_[position] == ']') return true;
        if (source_[position] != ',') return false;
        ++position;
        SkipWhitespace(position);
    }
    return false;
}

void TinyJson::SkipWhitespace(std::size_t& position) const {
    while (position < source_.size()) {
        const char character = source_[position];
        if (character != ' ' && character != '\t' && character != '\r' && character != '\n') break;
        ++position;
    }
}

bool TinyJson::IsHex(char character) {
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'f') ||
           (character >= 'A' && character <= 'F');
}

std::uint32_t TinyJson::HexValue(char character) {
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return 10 + character - 'a';
    return 10 + character - 'A';
}

void TinyJson::AppendUtf8(std::uint32_t codepoint, std::string& output) {
    if (codepoint <= 0x7f) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

bool TinyJson::ParseUnicode(std::size_t& position, std::uint32_t& codepoint) const {
    if (position + 4 > source_.size()) return false;
    codepoint = 0;
    for (int index = 0; index < 4; ++index) {
        if (!IsHex(source_[position + index])) return false;
        codepoint = (codepoint << 4) | HexValue(source_[position + index]);
    }
    position += 4;
    return true;
}

bool TinyJson::ParseString(std::size_t& position, std::string& output) const {
    if (position >= source_.size() || source_[position] != '"') return false;
    ++position;
    output.clear();
    while (position < source_.size()) {
        const unsigned char character = static_cast<unsigned char>(source_[position++]);
        if (character == '"') return true;
        if (character < 0x20) return false;
        if (character != '\\') {
            output.push_back(static_cast<char>(character));
            continue;
        }
        if (position >= source_.size()) return false;
        const char escaped = source_[position++];
        switch (escaped) {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u': {
                std::uint32_t first = 0;
                if (!ParseUnicode(position, first)) return false;
                if (first >= 0xd800 && first <= 0xdbff) {
                    if (position + 2 > source_.size() || source_[position] != '\\' ||
                        source_[position + 1] != 'u') return false;
                    position += 2;
                    std::uint32_t second = 0;
                    if (!ParseUnicode(position, second) || second < 0xdc00 || second > 0xdfff) {
                        return false;
                    }
                    first = 0x10000 + ((first - 0xd800) << 10) + (second - 0xdc00);
                } else if (first >= 0xdc00 && first <= 0xdfff) {
                    return false;
                }
                AppendUtf8(first, output);
                break;
            }
            default: return false;
        }
    }
    return false;
}

bool TinyJson::SkipNumber(std::size_t& position) const {
    const std::size_t start = position;
    if (position < source_.size() && source_[position] == '-') ++position;
    if (position >= source_.size()) return false;
    if (source_[position] == '0') {
        ++position;
    } else {
        if (source_[position] < '1' || source_[position] > '9') return false;
        while (position < source_.size() && source_[position] >= '0' &&
               source_[position] <= '9') ++position;
    }
    if (position < source_.size() && source_[position] == '.') {
        ++position;
        const std::size_t fraction = position;
        while (position < source_.size() && source_[position] >= '0' &&
               source_[position] <= '9') ++position;
        if (position == fraction) return false;
    }
    if (position < source_.size() && (source_[position] == 'e' || source_[position] == 'E')) {
        ++position;
        if (position < source_.size() && (source_[position] == '+' || source_[position] == '-')) {
            ++position;
        }
        const std::size_t exponent = position;
        while (position < source_.size() && source_[position] >= '0' &&
               source_[position] <= '9') ++position;
        if (position == exponent) return false;
    }
    return position > start;
}

bool TinyJson::SkipValue(std::size_t& position, int depth) const {
    if (depth > 32) return false;
    SkipWhitespace(position);
    if (position >= source_.size()) return false;
    if (source_[position] == '"') {
        std::string ignored;
        return ParseString(position, ignored);
    }
    if (source_[position] == '{') {
        ++position;
        SkipWhitespace(position);
        if (position < source_.size() && source_[position] == '}') {
            ++position;
            return true;
        }
        while (position < source_.size()) {
            std::string ignored;
            if (!ParseString(position, ignored)) return false;
            SkipWhitespace(position);
            if (position >= source_.size() || source_[position++] != ':') return false;
            if (!SkipValue(position, depth + 1)) return false;
            SkipWhitespace(position);
            if (position >= source_.size()) return false;
            if (source_[position] == '}') {
                ++position;
                return true;
            }
            if (source_[position++] != ',') return false;
            SkipWhitespace(position);
        }
        return false;
    }
    if (source_[position] == '[') {
        ++position;
        SkipWhitespace(position);
        if (position < source_.size() && source_[position] == ']') {
            ++position;
            return true;
        }
        while (position < source_.size()) {
            if (!SkipValue(position, depth + 1)) return false;
            SkipWhitespace(position);
            if (position >= source_.size()) return false;
            if (source_[position] == ']') {
                ++position;
                return true;
            }
            if (source_[position++] != ',') return false;
            SkipWhitespace(position);
        }
        return false;
    }
    if (source_.compare(position, 4, "true") == 0 ||
        source_.compare(position, 4, "null") == 0) {
        position += 4;
        return true;
    }
    if (source_.compare(position, 5, "false") == 0) {
        position += 5;
        return true;
    }
    return SkipNumber(position);
}

bool TinyJson::FindMember(const ObjectRange& object,
                          const std::string& key,
                          std::size_t& value_position) const {
    if (!object.valid || object.begin >= source_.size() || source_[object.begin] != '{') {
        return false;
    }
    std::size_t position = object.begin + 1;
    SkipWhitespace(position);
    if (position < object.end && source_[position] == '}') return false;
    while (position < object.end) {
        std::string member_name;
        if (!ParseString(position, member_name)) return false;
        SkipWhitespace(position);
        if (position >= object.end || source_[position++] != ':') return false;
        SkipWhitespace(position);
        if (member_name == key) {
            value_position = position;
            return true;
        }
        if (!SkipValue(position, 1)) return false;
        SkipWhitespace(position);
        if (position >= object.end || source_[position] == '}') return false;
        if (source_[position++] != ',') return false;
        SkipWhitespace(position);
    }
    return false;
}

}  // namespace clockin
