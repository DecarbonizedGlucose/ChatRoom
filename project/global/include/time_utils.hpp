#pragma once

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace TimeUtils {

    inline std::string timestamp_to_string(std::time_t timestamp) {
        std::tm* tm_info = std::localtime(&timestamp);
        std::ostringstream oss;
        oss << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    inline std::string timepoint_to_string(const std::chrono::system_clock::time_point& timepoint) {
        std::time_t timestamp = std::chrono::system_clock::to_time_t(timepoint);
        return timestamp_to_string(timestamp);
    }

    inline std::string current_time_string() {
        auto now = std::chrono::system_clock::now();
        return timepoint_to_string(now);
    }

} // namespace TimeUtils
