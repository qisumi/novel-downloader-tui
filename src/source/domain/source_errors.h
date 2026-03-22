#pragma once

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fanqie {

enum class SourceErrorCode {
    PluginLoadFailed,
    PluginInvalidManifest,
    PluginMissingMethod,
    PluginConfigError,
    PluginRuntimeError,
    PluginRequestError,
    PluginDataError,
    InvalidReturnType,
    InvalidReturnField,
    NetworkError,
    SourceNotSelected,
};

struct SourceError {
    SourceErrorCode code = SourceErrorCode::PluginRuntimeError;
    std::string     source_id;
    std::string     plugin_path;
    std::string     operation;
    std::string     message;
};

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

inline std::optional<std::string_view> source_error_prefix(SourceErrorCode code) {
    switch (code) {
    case SourceErrorCode::PluginConfigError:
        return "__fanqie_config_error__:";
    case SourceErrorCode::PluginRequestError:
        return "__fanqie_request_error__:";
    case SourceErrorCode::PluginDataError:
        return "__fanqie_data_error__:";
    case SourceErrorCode::NetworkError:
        return "__fanqie_network_error__:";
    default:
        return std::nullopt;
    }
}

inline std::string prefix_source_error(SourceErrorCode code, std::string_view message) {
    auto prefix = source_error_prefix(code);
    if (!prefix) {
        return std::string(message);
    }
    return std::string(*prefix) + std::string(message);
}

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

inline std::string strip_source_error_prefix(std::string_view message) {
    if (auto code = classify_prefixed_source_error(message)) {
        auto prefix = source_error_prefix(*code);
        return std::string(message.substr(prefix->size()));
    }
    return std::string(message);
}

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

class SourceException : public std::runtime_error {
public:
    explicit SourceException(SourceError error)
        : std::runtime_error(format_source_error(error)), error_(std::move(error)) {}

    const SourceError& error() const { return error_; }

private:
    SourceError error_;
};

} // namespace fanqie
