#pragma once

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace TimeUtils {

/**
 * 将时间戳转换为 [YYYY-MM-DD HH:MM:SS] 格式的字符串
 * @param timestamp Unix时间戳（秒）
 * @return 格式化后的时间字符串
 */
inline std::string timestamp_to_string(std::time_t timestamp) {
    std::tm* tm_info = std::localtime(&timestamp);
    std::ostringstream oss;
    oss << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

/**
 * 将时间戳转换为 [YYYY-MM-DD HH:MM:SS] 格式的字符串
 * @param timestamp_ms Unix时间戳（毫秒）
 * @return 格式化后的时间字符串
 */
inline std::string timestamp_ms_to_string(int64_t timestamp_ms) {
    std::time_t timestamp = timestamp_ms / 1000;
    return timestamp_to_string(timestamp);
}

/**
 * 将chrono时间点转换为 [YYYY-MM-DD HH:MM:SS] 格式的字符串
 * @param timepoint chrono时间点
 * @return 格式化后的时间字符串
 */
inline std::string timepoint_to_string(const std::chrono::system_clock::time_point& timepoint) {
    std::time_t timestamp = std::chrono::system_clock::to_time_t(timepoint);
    return timestamp_to_string(timestamp);
}

/**
 * 获取当前时间的 [YYYY-MM-DD HH:MM:SS] 格式字符串
 * @return 当前时间的格式化字符串
 */
inline std::string current_time_string() {
    auto now = std::chrono::system_clock::now();
    return timepoint_to_string(now);
}

/**
 * 获取当前Unix时间戳（秒）
 * @return 当前时间戳
 */
inline std::time_t current_timestamp() {
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

/**
 * 获取当前Unix时间戳（毫秒）
 * @return 当前时间戳（毫秒）
 */
inline int64_t current_timestamp_ms() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

/**
 * 将时间戳转换为不带括号的 YYYY-MM-DD HH:MM:SS 格式
 * @param timestamp Unix时间戳（秒）
 * @return 格式化后的时间字符串（不带括号）
 */
inline std::string timestamp_to_string_plain(std::time_t timestamp) {
    std::tm* tm_info = std::localtime(&timestamp);
    std::ostringstream oss;
    oss << std::put_time(tm_info, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace TimeUtils
