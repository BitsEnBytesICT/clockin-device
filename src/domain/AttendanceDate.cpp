#include "domain/AttendanceDate.hpp"

#include <cctype>

namespace clockin {
namespace {

bool IsDigit(char value) {
    return std::isdigit(static_cast<unsigned char>(value)) != 0;
}

int TwoDigits(const std::string& value, std::size_t offset) {
    return (value[offset] - '0') * 10 + value[offset + 1] - '0';
}

bool IsLeapYear(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

bool HasValidDatePrefix(const std::string& value) {
    if (value.size() < 10 || value[4] != '-' || value[7] != '-') return false;
    for (std::size_t index : {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U}) {
        if (!IsDigit(value[index])) return false;
    }
    const int year = (value[0] - '0') * 1000 + (value[1] - '0') * 100 +
                     (value[2] - '0') * 10 + value[3] - '0';
    const int month = TwoDigits(value, 5);
    const int day = TwoDigits(value, 8);
    if (year == 0 || month < 1 || month > 12 || day < 1) return false;
    constexpr int days_per_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    const int maximum_day = month == 2 && IsLeapYear(year)
                                ? 29
                                : days_per_month[month - 1];
    return day <= maximum_day;
}

}  // namespace

std::string FormatAttendanceDate(const std::string& value) {
    if (!HasValidDatePrefix(value)) return value;
    if (value.size() == 10 || value[10] == 'T' || value[10] == ' ') {
        return value.substr(0, 10);
    }
    return value;
}

}  // namespace clockin
