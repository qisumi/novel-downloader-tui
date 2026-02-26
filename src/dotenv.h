/**
 * dotenv.h — 轻量级 .env 文件解析器
 *
 * 使用方式：
 *   dotenv::load(".env");          // 加载 .env，不覆盖已有环境变量
 *   dotenv::load(".env", true);    // 加载 .env，覆盖已有环境变量
 *
 * 格式支持：
 *   KEY=VALUE
 *   KEY="VALUE WITH SPACES"
 *   KEY='VALUE WITH SPACES'
 *   # 注释行
 *   空行忽略
 *
 * 优先级（不覆盖模式，默认）：
 *   命令行参数 > 系统环境变量 > .env 文件
 */
#pragma once

#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

#ifdef _WIN32
#  include <stdlib.h>  // _putenv_s
#endif

namespace dotenv {

namespace detail {

/// 去除字符串首尾空白
inline std::string trim(std::string_view sv) {
    const auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return {};
    const auto end = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(start, end - start + 1));
}

/// 去除首尾引号（单引号或双引号），仅当首尾引号匹配时
inline std::string unquote(std::string_view sv) {
    if (sv.size() >= 2) {
        const char q = sv.front();
        if ((q == '"' || q == '\'') && sv.back() == q) {
            return std::string(sv.substr(1, sv.size() - 2));
        }
    }
    return std::string(sv);
}

/// 设置环境变量（跨平台）
inline bool set_env(const std::string& key, const std::string& value) {
#ifdef _WIN32
    return ::_putenv_s(key.c_str(), value.c_str()) == 0;
#else
    return ::setenv(key.c_str(), value.c_str(), 1) == 0;
#endif
}

/// 检查环境变量是否已存在
inline bool env_exists(const std::string& key) {
    return std::getenv(key.c_str()) != nullptr;
}

}  // namespace detail

/**
 * 从指定路径加载 .env 文件。
 *
 * @param path      .env 文件路径，默认 ".env"
 * @param overwrite true  = 覆盖已有环境变量（.env 优先级最高，一般不用）
 *                  false = 不覆盖（默认，最终优先级：命令行 > 系统环境变量 > .env）
 * @return 成功加载的键值对数量，文件不存在时返回 -1
 */
inline int load(const std::string& path = ".env", bool overwrite = false) {
    std::ifstream file(path);
    if (!file.is_open()) return -1;

    int count = 0;
    std::string line;
    while (std::getline(file, line)) {
        // 去空白
        const auto trimmed = detail::trim(line);
        // 跳过空行与注释
        if (trimmed.empty() || trimmed.front() == '#') continue;

        // 找第一个 '='
        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        auto key   = detail::trim(trimmed.substr(0, eq));
        auto value = detail::unquote(detail::trim(trimmed.substr(eq + 1)));

        if (key.empty()) continue;

        // 行内注释：去掉未被引号包围的 # 之后的内容
        // （注：已经 unquote，简单处理：若值无引号则找 ' #'）
        // 由于 unquote 已处理带引号值，这里只处理裸值的行内注释
        if (!trimmed.substr(eq + 1).empty()) {
            const auto raw_val = detail::trim(trimmed.substr(eq + 1));
            if (!raw_val.empty() && raw_val.front() != '"' && raw_val.front() != '\'') {
                const auto comment = raw_val.find(" #");
                if (comment != std::string::npos) {
                    value = detail::trim(raw_val.substr(0, comment));
                }
            }
        }

        if (!overwrite && detail::env_exists(key)) continue;

        if (detail::set_env(key, value)) ++count;
    }
    return count;
}

/**
 * 解析 .env 文件并返回键值对 map，不写入环境变量。
 */
inline std::unordered_map<std::string, std::string> parse(const std::string& path = ".env") {
    std::unordered_map<std::string, std::string> result;
    std::ifstream file(path);
    if (!file.is_open()) return result;

    std::string line;
    while (std::getline(file, line)) {
        const auto trimmed = detail::trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;
        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;
        auto key   = detail::trim(trimmed.substr(0, eq));
        auto value = detail::unquote(detail::trim(trimmed.substr(eq + 1)));
        if (!key.empty()) result.emplace(std::move(key), std::move(value));
    }
    return result;
}

}  // namespace dotenv
