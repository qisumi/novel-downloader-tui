#pragma once

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace novel {

/// 书源错误码枚举，涵盖插件加载、运行时、数据校验等错误类型
enum class SourceErrorCode {
    PluginLoadFailed,         ///< 插件加载失败
    PluginInvalidManifest,    ///< 插件清单(manifest)无效
    PluginMissingMethod,      ///< 插件缺少必要方法
    PluginConfigError,        ///< 插件配置错误
    PluginRuntimeError,       ///< 插件运行时错误
    PluginRequestError,       ///< 插件请求参数错误
    PluginDataError,          ///< 插件数据处理错误
    InvalidReturnType,        ///< 插件返回值类型错误
    InvalidReturnField,       ///< 插件返回值字段错误
    NetworkError,             ///< 网络请求错误
    SourceNotSelected,        ///< 未选择书源
};

/// 书源错误详情结构体
struct SourceError {
    SourceErrorCode code = SourceErrorCode::PluginRuntimeError; ///< 错误码
    std::string     source_id;    ///< 所属书源 ID
    std::string     plugin_path;  ///< 插件文件路径
    std::string     operation;    ///< 出错的操作名（如 "search"、"get_toc"）
    std::string     message;      ///< 错误描述信息
};

/// 将错误码转换为字符串名称，用于日志输出
inline std::string_view source_error_code_name(SourceErrorCode code) {
    switch (code) {
    case SourceErrorCode::PluginLoadFailed:
        return "plugin_load_failed";
    case SourceErrorCode::PluginInvalidManifest:
        return "plugin_invalid_manifest";
    case SourceErrorCode::PluginMissingMethod:
        return "plugin_missing_method";
    case SourceErrorCode::PluginConfigError:
        return "plugin_config_error";
    case SourceErrorCode::PluginRuntimeError:
        return "plugin_runtime_error";
    case SourceErrorCode::PluginRequestError:
        return "plugin_request_error";
    case SourceErrorCode::PluginDataError:
        return "plugin_data_error";
    case SourceErrorCode::InvalidReturnType:
        return "invalid_return_type";
    case SourceErrorCode::InvalidReturnField:
        return "invalid_return_field";
    case SourceErrorCode::NetworkError:
        return "network_error";
    case SourceErrorCode::SourceNotSelected:
        return "source_not_selected";
    }
    return "unknown_error";
}

/// 获取特定错误码的前缀字符串，用于在 JS↔C++ 之间传递分类错误信息
/// 部分 JS 端抛出的错误会带有前缀标记，C++ 端据此分类
inline std::optional<std::string_view> source_error_prefix(SourceErrorCode code) {
    switch (code) {
    case SourceErrorCode::PluginConfigError:
        return "__novel_config_error__:";
    case SourceErrorCode::PluginRequestError:
        return "__novel_request_error__:";
    case SourceErrorCode::PluginDataError:
        return "__novel_data_error__:";
    case SourceErrorCode::NetworkError:
        return "__novel_network_error__:";
    default:
        return std::nullopt;
    }
}

/// 为错误消息添加前缀标记
inline std::string prefix_source_error(SourceErrorCode code, std::string_view message) {
    auto prefix = source_error_prefix(code);
    if (!prefix) {
        return std::string(message);
    }
    return std::string(*prefix) + std::string(message);
}

/// 根据消息前缀分类错误码（反向解析带前缀的错误消息）
inline std::optional<SourceErrorCode> classify_prefixed_source_error(std::string_view message) {
    for (SourceErrorCode code : {
             SourceErrorCode::PluginConfigError,
             SourceErrorCode::PluginRequestError,
             SourceErrorCode::PluginDataError,
             SourceErrorCode::NetworkError,
         }) {
        auto prefix = source_error_prefix(code);
        if (prefix && message.starts_with(*prefix)) {
            return code;
        }
    }
    return std::nullopt;
}

/// 去除错误消息中的前缀标记，返回纯文本
inline std::string strip_source_error_prefix(std::string_view message) {
    if (auto code = classify_prefixed_source_error(message)) {
        auto prefix = source_error_prefix(*code);
        return std::string(message.substr(prefix->size()));
    }
    return std::string(message);
}

/// 将 SourceError 格式化为面向用户的中文错误描述
inline std::string format_source_error(const SourceError& error) {
    std::ostringstream oss;

    oss << "书源";
    if (!error.source_id.empty()) {
        oss << "[" << error.source_id << "]";
    }

    switch (error.code) {
    case SourceErrorCode::PluginConfigError:
        oss << "配置错误";
        break;
    case SourceErrorCode::PluginRequestError:
        oss << "请求参数错误";
        break;
    case SourceErrorCode::PluginDataError:
        oss << "数据处理错误";
        break;
    case SourceErrorCode::NetworkError:
        oss << "网络错误";
        break;
    case SourceErrorCode::InvalidReturnType:
    case SourceErrorCode::InvalidReturnField:
        oss << "返回数据格式错误";
        break;
    case SourceErrorCode::PluginLoadFailed:
        oss << "加载失败";
        break;
    case SourceErrorCode::PluginInvalidManifest:
        oss << "清单无效";
        break;
    case SourceErrorCode::PluginMissingMethod:
        oss << "缺少必要方法";
        break;
    case SourceErrorCode::SourceNotSelected:
        oss << "未选择书源";
        break;
    case SourceErrorCode::PluginRuntimeError:
    default:
        oss << "运行失败";
        break;
    }

    if (!error.operation.empty()) {
        oss << "（" << error.operation << "）";
    }
    if (!error.message.empty()) {
        oss << "：" << error.message;
    }
    return oss.str();
}

/// 将 SourceError 格式化为结构化日志文本，用于 spdlog 输出
inline std::string format_source_error_log(const SourceError& error) {
    std::ostringstream oss;
    oss << "code=" << source_error_code_name(error.code);
    if (!error.source_id.empty()) {
        oss << " source=" << error.source_id;
    }
    if (!error.operation.empty()) {
        oss << " operation=" << error.operation;
    }
    if (!error.plugin_path.empty()) {
        oss << " plugin=" << error.plugin_path;
    }
    if (!error.message.empty()) {
        oss << " message=" << error.message;
    }
    return oss.str();
}

/// 书源异常类，携带 SourceError 详情并可被 std::runtime_error 捕获
class SourceException : public std::runtime_error {
public:
    explicit SourceException(SourceError error)
        : std::runtime_error(format_source_error(error)), error_(std::move(error)) {}

    const SourceError& error() const { return error_; }

private:
    SourceError error_;
};

} // namespace novel
