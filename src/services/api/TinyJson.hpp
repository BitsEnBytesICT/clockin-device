#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace clockin {

class TinyJson {
public:
    struct ObjectRange {
        std::size_t begin = 0;
        std::size_t end = 0;
        bool valid = false;
    };

    explicit TinyJson(std::string source);

    bool RootObject(ObjectRange& output) const;
    bool GetString(const ObjectRange& object,
                   const std::string& key,
                   std::string& output) const;
    bool GetBool(const ObjectRange& object, const std::string& key, bool& output) const;
    bool GetObject(const ObjectRange& object,
                   const std::string& key,
                   ObjectRange& output) const;
    bool GetStringArray(const ObjectRange& object,
                        const std::string& key,
                        std::vector<std::string>& output) const;

private:
    std::string source_;

    void SkipWhitespace(std::size_t& position) const;
    static bool IsHex(char character);
    static std::uint32_t HexValue(char character);
    static void AppendUtf8(std::uint32_t codepoint, std::string& output);
    bool ParseUnicode(std::size_t& position, std::uint32_t& codepoint) const;
    bool ParseString(std::size_t& position, std::string& output) const;
    bool SkipNumber(std::size_t& position) const;
    bool SkipValue(std::size_t& position, int depth) const;
    bool FindMember(const ObjectRange& object,
                    const std::string& key,
                    std::size_t& value_position) const;
};

}  // namespace clockin
