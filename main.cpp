// Copyright (c) 2024 Devexperts LLC.
// SPDX-License-Identifier: MPL-2.0

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include <DXErrorCodes.h>
#include <DXFeed.h>

#ifdef _MSC_FULL_VER
#pragma warning(push)
#pragma warning(disable : 4244)
#endif

struct StringConverter {
    static std::string toString(const std::wstring &wstring) {
        return std::string(wstring.begin(), wstring.end());
    }

    static std::string toString(wchar_t wchar) {
        return toString(std::wstring(1, wchar));
    }

    template <typename InputIterator>
    static std::string toString(InputIterator first, InputIterator last) {
        return toString(std::wstring(first, last));
    }

    static std::wstring toWString(const std::string &string) {
        return std::wstring(string.begin(), string.end());
    }
};

#ifdef _MSC_FULL_VER
#pragma warning(pop)
#endif

enum TimeZone {
    LOCAL,
    GMT
};

template <TimeZone>
inline std::string formatTime(long long timestamp, const std::string &format = "%Y-%m-%d %H:%M:%S");

template <>
inline std::string formatTime<LOCAL>(long long timestamp, const std::string &format) {
    return fmt::format(fmt::format("{{:{}}}", format), fmt::localtime(static_cast<std::time_t>(timestamp)));
}

template <>
inline std::string formatTime<GMT>(long long timestamp, const std::string &format) {
    return fmt::format(fmt::format("{{:{}}}", format), fmt::gmtime(static_cast<std::time_t>(timestamp)));
}

template <TimeZone tz>
inline std::string formatTimestampWithMillis(long long timestamp) {
    long long ms = timestamp % 1000;

    return fmt::format("{}.{:0>3}", formatTime<tz>(timestamp / 1000), ms);
}

#define UNIQUE_NAME_LINE2(name, line) name##line
#define UNIQUE_NAME_LINE(name, line) UNIQUE_NAME_LINE2(name, line)
#define UNIQUE_NAME(name) UNIQUE_NAME_LINE(name, __LINE__)

namespace detail {
template <typename F>
constexpr auto onScopeExitImpl(F &&f) {
    auto onExit = [&f](auto) {
        f();
    };

    return std::shared_ptr<void>(nullptr, onExit);
}
} // namespace detail

#define onScopeExit(...) auto UNIQUE_NAME(FINALLY_) = detail::onScopeExitImpl(__VA_ARGS__)

inline void printTimestamp(dxf_long_t timestamp, dxf_const_string_t keyName = L"") {
    if (keyName && keyName[0] != 0) {
        std::wcout << keyName << " = ";
    }

    std::wcout << StringConverter::toWString(formatTimestampWithMillis<LOCAL>(timestamp)).c_str();
}

inline dxf_const_string_t orderScopeToString(dxf_order_scope_t scope) {
    switch (scope) {
    case dxf_osc_composite:
        return L"Composite";
    case dxf_osc_regional:
        return L"Regional";
    case dxf_osc_aggregate:
        return L"Aggregate";
    case dxf_osc_order:
        return L"Order";
    }

    return L"";
}

inline dxf_const_string_t orderSideToString(dxf_order_side_t side) {
    switch (side) {
    case dxf_osd_undefined:
        return L"Undefined";
    case dxf_osd_buy:
        return L"Buy";
    case dxf_osd_sell:
        return L"Sell";
    }

    return L"";
}

std::recursive_mutex ioMutex{};

inline void processLastError() {
    std::lock_guard<std::recursive_mutex> lock{ioMutex};

    int errorCode = dx_ec_success;

    dxf_const_string_t errorDescription = nullptr;
    auto res = dxf_get_last_error(&errorCode, &errorDescription);

    if (res == DXF_SUCCESS) {
        if (errorCode == dx_ec_success) {
            std::wcout << L"No error information is stored" << std::endl;

            return;
        }

        std::wcout << L"Error occurred and successfully retrieved:\nerror code = " << errorCode << ", description = \""
            << errorDescription << "\"" << std::endl;

        return;
    }

    std::wcout << L"An error occurred but the error subsystem failed to initialize" << std::endl;
}

using ListenerType = void(int /*eventType*/, dxf_const_string_t /*symbolName*/, const dxf_event_data_t * /*data*/,
                          int /*dataCount*/, void * /*userData*/);
using ListenerPtrType = std::add_pointer_t<ListenerType>;

// struct SubscriptionBase {
//     virtual ~SubscriptionBase() = default;
//     virtual void Close() = 0;
// };

template <typename F, typename... Args>
void log(F &&format, Args &&... args) {
    std::lock_guard<std::recursive_mutex> lock{ioMutex};
    fmt::print(format, args...);
}


// https://stackoverflow.com/a/58037981/21913386
// Algorithm: http://howardhinnant.github.io/date_algorithms.html
int daysFromEpoch(int y, int m, int d) {
    y -= m <= 2;
    int era = y / 400;
    int yoe = y - era * 400; // [0, 399]
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
    return era * 146097 + doe - 719468;
}

// It  does not modify broken-down time
time_t timegm(struct tm const *t) {
    int year = t->tm_year + 1900;
    int month = t->tm_mon; // 0-11
    if (month > 11) {
        year += month / 12;
        month %= 12;
    } else if (month < 0) {
        int years_diff = (11 - month) / 12;
        year -= years_diff;
        month += 12 * years_diff;
    }
    int days_since_epoch = daysFromEpoch(year, month + 1, t->tm_mday);

    return 60 * (60 * (24L * days_since_epoch + t->tm_hour) + t->tm_min) + t->tm_sec;
}

// Function to convert a tm structure to a time_point in UTC/GMT
std::chrono::system_clock::time_point tmToUTCTimePoint(std::tm *tm) {
    // Construct time_t from tm as if it is UTC time
    time_t utcTime = timegm(tm);
    // Convert time_t to system_clock::time_point
    return std::chrono::system_clock::from_time_t(utcTime);
}


template <TimeZone>
inline long long parseDateTime(const std::string &dateTimeString, const std::string &format);

template <>
inline long long parseDateTime<LOCAL>(const std::string &dateTimeString, const std::string &format) {
    std::tm tm = {};
    std::istringstream ss(dateTimeString);

    ss >> std::get_time(&tm, format.c_str());

    if (ss.fail()) {
        return std::numeric_limits<long long>::min();
    }

    return std::chrono::system_clock::from_time_t(std::mktime(&tm)).time_since_epoch().count() * 1000;
}

template <>
inline long long parseDateTime<GMT>(const std::string &dateTimeString, const std::string &format) {
    std::tm tm = {};
    std::istringstream ss(dateTimeString);

    ss >> std::get_time(&tm, format.c_str());

    if (ss.fail()) {
        return std::numeric_limits<long long>::min();
    }

    return tmToUTCTimePoint(&tm).time_since_epoch().count() * 1000;
}

/*
 * Parse date string in format '%Y-%m-%d %H:%M:%S'
 */
long long parseDateTime(const std::string &dateTimeString) {
    if (dateTimeString.size() > 1 && dateTimeString.back() == 'Z') {
        return parseDateTime<GMT>(dateTimeString.substr(0, dateTimeString.size() - 1), "%Y-%m-%d %H:%M:%S");
    }

    return parseDateTime<LOCAL>(dateTimeString.substr(0, dateTimeString.size() - 1), "%Y-%m-%d %H:%M:%S");
}


int main(int argc, char *argv[]) {
    std::string address = "demo.dxfeed.com:7300";

    if (argc > 1) {
        address = argv[1];
    }

    std::string symbol = "AAPL&Q{=m}";

    if (argc > 2) {
        symbol = argv[2];
    }

    std::cout << "Hello, World!" << argc << std::endl;
    return 0;
}