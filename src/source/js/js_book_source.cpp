#include "source/js/js_book_source.h"

#include <cstdint>
#include <type_traits>
#include <utility>

#include "source/domain/source_errors.h"

namespace novel {

namespace {

using json = nlohmann::json;

/// 获取 JSON 值的可读类型名称，用于错误提示
std::string json_type_name(const json& value) {
    if (value.is_null()) {
        return "null";
    }
    if (value.is_boolean()) {
        return "boolean";
    }
    if (value.is_number_integer()) {
        return "integer";
    }
    if (value.is_number_unsigned()) {
        return "unsigned";
    }
    if (value.is_number_float()) {
        return "number";
    }
    if (value.is_string()) {
        return "string";
    }
    if (value.is_array()) {
        return "array";
    }
    if (value.is_object()) {
        return "object";
    }
    return "unknown";
}

/// 构造并抛出一个包含完整上下文信息的 SourceException
[[noreturn]] void throw_invalid_return(
    SourceErrorCode code,
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& field,
    const std::string& message) {
    throw SourceException({code, source_id, plugin_path, field, message});
}

/// 从 JSON 值中提取字符串，兼容字符串/整数/浮点/布尔类型
/// 若值为 null 且 required=true，则抛出异常；否则返回 fallback
std::string string_from_value(
    const json& value,
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& field,
    bool required,
    const std::string& fallback = {}) {
    if (value.is_null()) {
        if (required) {
            throw_invalid_return(
                SourceErrorCode::InvalidReturnField,
                source_id,
                plugin_path,
                field,
                "required field is missing");
        }
        return fallback;
    }

    if (value.is_string()) {
        auto text = value.get<std::string>();
        if (required && text.empty()) {
            throw_invalid_return(
                SourceErrorCode::InvalidReturnField,
                source_id,
                plugin_path,
                field,
                "required field cannot be empty");
        }
        return text;
    }
    if (value.is_number_integer()) {
        return std::to_string(value.get<std::int64_t>());
    }
    if (value.is_number_unsigned()) {
        return std::to_string(value.get<std::uint64_t>());
    }
    if (value.is_number_float()) {
        return std::to_string(value.get<double>());
    }
    if (value.is_boolean()) {
        return value.get<bool>() ? "true" : "false";
    }

    throw_invalid_return(
        SourceErrorCode::InvalidReturnField,
        source_id,
        plugin_path,
        field,
        "expected string-compatible value, got " + json_type_name(value));
}

/// 从 JSON 值中提取浮点数，兼容数值和字符串类型
double number_from_value(
    const json& value,
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& field,
    double fallback = 0.0) {
    if (value.is_null()) {
        return fallback;
    }
    if (value.is_number()) {
        return value.get<double>();
    }
    if (value.is_string()) {
        try {
            return std::stod(value.get<std::string>());
        } catch (...) {
        }
    }

    throw_invalid_return(
        SourceErrorCode::InvalidReturnField,
        source_id,
        plugin_path,
        field,
        "expected numeric value, got " + json_type_name(value));
}

/// 从 JSON 值中提取整数，兼容整数/无符号整数/浮点/字符串类型
/// @tparam IntT 目标整数类型（int / int64_t 等）
template <typename IntT>
IntT integer_from_value(
    const json& value,
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& field,
    IntT fallback = 0) {
    if (value.is_null()) {
        return fallback;
    }
    if (value.is_number_integer()) {
        return static_cast<IntT>(value.get<std::int64_t>());
    }
    if (value.is_number_unsigned()) {
        return static_cast<IntT>(value.get<std::uint64_t>());
    }
    if (value.is_number_float()) {
        return static_cast<IntT>(value.get<double>());
    }
    if (value.is_string()) {
        try {
            if constexpr (std::is_same_v<IntT, std::int64_t>) {
                return static_cast<IntT>(std::stoll(value.get<std::string>()));
            } else {
                return static_cast<IntT>(std::stoi(value.get<std::string>()));
            }
        } catch (...) {
        }
    }

    throw_invalid_return(
        SourceErrorCode::InvalidReturnField,
        source_id,
        plugin_path,
        field,
        "expected integer value, got " + json_type_name(value));
}

/// 从 JSON 值中提取字符串数组
/// 若值不是数组则抛出 PluginInvalidManifest 异常
std::vector<std::string> string_list_from_value(
    const json& value,
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& field) {
    if (value.is_null()) {
        return {};
    }
    if (!value.is_array()) {
        throw_invalid_return(
            SourceErrorCode::PluginInvalidManifest,
            source_id,
            plugin_path,
            field,
            "expected string array");
    }

    std::vector<std::string> items;
    items.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        const auto item_field = field + "[" + std::to_string(index) + "]";
        items.push_back(string_from_value(value[index], source_id, plugin_path, item_field, false));
    }
    return items;
}

/// 从 JSON 对象解析 Book 结构体，校验所有字段类型
Book parse_book(
    const json& value,
    const std::string& source_id,
    const std::string& plugin_path) {
    if (!value.is_object()) {
        throw_invalid_return(
            SourceErrorCode::InvalidReturnType,
            source_id,
            plugin_path,
            "book",
            "expected object");
    }

    Book book;
    book.book_id = string_from_value(value.value("book_id", json(nullptr)), source_id, plugin_path, "book_id", true);
    book.title = string_from_value(value.value("title", json(nullptr)), source_id, plugin_path, "title", true);
    book.author = string_from_value(value.value("author", json(nullptr)), source_id, plugin_path, "author", false);
    book.cover_url = string_from_value(value.value("cover_url", json(nullptr)), source_id, plugin_path, "cover_url", false);
    book.abstract = string_from_value(value.value("abstract", json(nullptr)), source_id, plugin_path, "abstract", false);
    book.category = string_from_value(value.value("category", json(nullptr)), source_id, plugin_path, "category", false);
    book.word_count = string_from_value(value.value("word_count", json(nullptr)), source_id, plugin_path, "word_count", false);
    book.score = number_from_value(value.value("score", json(nullptr)), source_id, plugin_path, "score", 0.0);
    book.gender = integer_from_value<int>(value.value("gender", json(nullptr)), source_id, plugin_path, "gender", 0);
    book.creation_status = integer_from_value<int>(
        value.value("creation_status", json(nullptr)),
        source_id,
        plugin_path,
        "creation_status",
        0);
    book.last_chapter_title = string_from_value(
        value.value("last_chapter_title", json(nullptr)),
        source_id,
        plugin_path,
        "last_chapter_title",
        false);
    book.last_update_time = integer_from_value<std::int64_t>(
        value.value("last_update_time", json(nullptr)),
        source_id,
        plugin_path,
        "last_update_time",
        0);
    return book;
}

/// 从 JSON 对象解析 TocItem（目录条目）结构体
TocItem parse_toc_item(
    const json& value,
    const std::string& source_id,
    const std::string& plugin_path) {
    if (!value.is_object()) {
        throw_invalid_return(
            SourceErrorCode::InvalidReturnType,
            source_id,
            plugin_path,
            "toc_item",
            "expected object");
    }

    TocItem item;
    item.item_id = string_from_value(value.value("item_id", json(nullptr)), source_id, plugin_path, "item_id", true);
    item.title = string_from_value(value.value("title", json(nullptr)), source_id, plugin_path, "title", true);
    item.volume_name = string_from_value(value.value("volume_name", json(nullptr)), source_id, plugin_path, "volume_name", false);
    item.word_count = integer_from_value<int>(
        value.value("word_count", json(nullptr)),
        source_id,
        plugin_path,
        "word_count",
        0);
    item.update_time = integer_from_value<std::int64_t>(
        value.value("update_time", json(nullptr)),
        source_id,
        plugin_path,
        "update_time",
        0);
    return item;
}

/// 从 JSON 对象解析 Chapter（章节正文）结构体
Chapter parse_chapter(
    const json& value,
    const std::string& source_id,
    const std::string& plugin_path) {
    if (!value.is_object()) {
        throw_invalid_return(
            SourceErrorCode::InvalidReturnType,
            source_id,
            plugin_path,
            "chapter",
            "expected object");
    }

    Chapter chapter;
    chapter.item_id = string_from_value(value.value("item_id", json(nullptr)), source_id, plugin_path, "item_id", false);
    chapter.title = string_from_value(value.value("title", json(nullptr)), source_id, plugin_path, "title", false);
    chapter.content = string_from_value(value.value("content", json(nullptr)), source_id, plugin_path, "content", true);
    return chapter;
}

/// 带错误上下文的调用包装器：捕获异常并补充 source_id / plugin_path / operation 信息
/// 确保所有异常都以 SourceException 形式抛出，携带完整调试上下文
template <typename Fn>
auto invoke_with_error_context(
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& operation,
    Fn&& fn) -> decltype(fn()) {
    try {
        return fn();
    } catch (const SourceException& e) {
        // 补全 SourceException 中缺失的上下文字段
        auto error = e.error();
        if (error.source_id.empty()) {
            error.source_id = source_id;
        }
        if (error.plugin_path.empty()) {
            error.plugin_path = plugin_path;
        }
        if (error.operation.empty()) {
            error.operation = operation;
        }
        throw SourceException(std::move(error));
    } catch (const std::exception& e) {
        // 将普通异常转换为 SourceException，尝试从消息前缀推断错误码
        std::string message = e.what();
        const auto code = classify_prefixed_source_error(message)
                              .value_or(SourceErrorCode::PluginRuntimeError);
        throw SourceException({
            code,
            source_id,
            plugin_path,
            operation,
            strip_source_error_prefix(message),
        });
    }
}

} // namespace

// ── 构造与初始化 ─────────────

/// 从引导阶段的插件信息构造 JS 书源
/// 解析 manifest，并校验插件是否实现了必要方法（search / get_toc / get_chapter）
JsBookSource::JsBookSource(std::shared_ptr<JsPluginRuntime> runtime, const JsBootstrapPlugin& plugin)
    : plugin_path_(plugin.plugin_path),
      module_id_(plugin.module_id),
      runtime_(std::move(runtime)),
      has_configure_(plugin.has_configure),
      has_login_(plugin.has_login),
      has_status_(plugin.has_status),
      has_book_info_(plugin.has_book_info),
      has_batch_count_(plugin.has_batch_count),
      has_batch_(plugin.has_batch) {
    load_manifest(plugin.manifest);
    capabilities_.supports_book_info = has_book_info_;
    capabilities_.supports_batch = has_batch_count_ && has_batch_;
    capabilities_.supports_login = has_login_;

    // 校验插件是否导出了必需的方法
    if (!plugin.has_search) {
        throw SourceException({
            SourceErrorCode::PluginMissingMethod,
            info_.id,
            plugin_path_,
            "search",
            "missing required plugin function",
        });
    }
    if (!plugin.has_toc) {
        throw SourceException({
            SourceErrorCode::PluginMissingMethod,
            info_.id,
            plugin_path_,
            "get_toc",
            "missing required plugin function",
        });
    }
    if (!plugin.has_chapter) {
        throw SourceException({
            SourceErrorCode::PluginMissingMethod,
            info_.id,
            plugin_path_,
            "get_chapter",
            "missing required plugin function",
        });
    }
}

// ── IBookSource 接口实现 ─────────────

/// 调用 JS 插件的 configure 方法（若插件实现了该方法）
void JsBookSource::configure() {
    logged_in_ = false;
    session_status_ = {};

    if (!has_configure_) {
        return;
    }

    invoke_with_error_context(info_.id, plugin_path_, "configure", [&] {
        runtime_->call(module_id_, "configure", json::array());
        return 0;
    });
}

/// 调用 JS 插件的 login 方法（若插件实现了该方法）
bool JsBookSource::login() {
    if (!has_login_) {
        return false;
    }

    logged_in_ = false;
    session_status_ = {};
    logged_in_ = invoke_with_error_context(info_.id, plugin_path_, "login", [&] {
        const auto result = runtime_->call(module_id_, "login", json::array());
        if (!result.is_boolean()) {
            throw SourceException({
                SourceErrorCode::InvalidReturnType,
                info_.id,
                plugin_path_,
                "login",
                "expected boolean",
            });
        }
        return result.get<bool>();
    });
    session_status_.logged_in = logged_in_;
    return logged_in_;
}

SourceSessionStatus JsBookSource::get_session_status() {
    if (!has_status_) {
        session_status_.logged_in = logged_in_;
        if (!logged_in_) {
            session_status_.remaining_download_quota.reset();
        }
        return session_status_;
    }

    return query_session_status(false);
}

SourceSessionStatus JsBookSource::refresh_session_status() {
    if (!has_status_) {
        session_status_.logged_in = logged_in_;
        if (!logged_in_) {
            session_status_.remaining_download_quota.reset();
        }
        return session_status_;
    }

    return query_session_status(true);
}

/// 调用 JS 插件的 search 方法，解析返回的书籍数组
std::vector<Book> JsBookSource::search(const std::string& keywords, int page) {
    return invoke_with_error_context(info_.id, plugin_path_, "search", [&] {
        const auto result = runtime_->call(module_id_, "search", json::array({keywords, page}));
        if (!result.is_array()) {
            throw SourceException({
                SourceErrorCode::InvalidReturnType,
                info_.id,
                plugin_path_,
                "search",
                "expected array",
            });
        }

        std::vector<Book> books;
        books.reserve(result.size());
        for (const auto& item : result) {
            books.push_back(parse_book(item, info_.id, plugin_path_));
        }
        return books;
    });
}

/// 调用 JS 插件的 get_book_info 方法，返回单本书籍详情（可选实现）
std::optional<Book> JsBookSource::get_book_info(const std::string& book_id) {
    if (!has_book_info_) {
        return std::nullopt;
    }

    return invoke_with_error_context(info_.id, plugin_path_, "get_book_info", [&]() -> std::optional<Book> {
        const auto result = runtime_->call(module_id_, "get_book_info", json::array({book_id}));
        if (result.is_null()) {
            return std::nullopt;
        }
        return parse_book(result, info_.id, plugin_path_);
    });
}

/// 调用 JS 插件的 get_toc 方法，返回指定书籍的目录列表
std::vector<TocItem> JsBookSource::get_toc(const std::string& book_id) {
    return invoke_with_error_context(info_.id, plugin_path_, "get_toc", [&] {
        const auto result = runtime_->call(module_id_, "get_toc", json::array({book_id}));
        if (!result.is_array()) {
            throw SourceException({
                SourceErrorCode::InvalidReturnType,
                info_.id,
                plugin_path_,
                "get_toc",
                "expected array",
            });
        }

        std::vector<TocItem> items;
        items.reserve(result.size());
        for (const auto& item : result) {
            items.push_back(parse_toc_item(item, info_.id, plugin_path_));
        }
        return items;
    });
}

/// 调用 JS 插件的 get_chapter 方法，返回指定章节的正文内容
std::optional<Chapter> JsBookSource::get_chapter(
    const std::string& book_id,
    const std::string& item_id) {
    return invoke_with_error_context(info_.id, plugin_path_, "get_chapter", [&]() -> std::optional<Chapter> {
        const auto result = runtime_->call(module_id_, "get_chapter", json::array({book_id, item_id}));
        if (result.is_null()) {
            return std::nullopt;
        }
        return parse_chapter(result, info_.id, plugin_path_);
    });
}

/// 调用 JS 插件的 get_batch_count 方法，返回总批次数
int JsBookSource::get_batch_count(const std::string& book_id) {
    if (!has_batch_count_) {
        return 0;
    }

    return invoke_with_error_context(info_.id, plugin_path_, "get_batch_count", [&] {
        const auto result = runtime_->call(module_id_, "get_batch_count", json::array({book_id}));
        return integer_from_value<int>(result, info_.id, plugin_path_, "get_batch_count", 0);
    });
}

/// 调用 JS 插件的 get_batch 方法，返回指定批次的章节列表
std::vector<Chapter> JsBookSource::get_batch(const std::string& book_id, int batch_no) {
    if (!has_batch_) {
        return {};
    }

    return invoke_with_error_context(info_.id, plugin_path_, "get_batch", [&] {
        const auto result = runtime_->call(module_id_, "get_batch", json::array({book_id, batch_no}));
        if (result.is_null()) {
            return std::vector<Chapter>{};
        }
        if (!result.is_array()) {
            throw SourceException({
                SourceErrorCode::InvalidReturnType,
                info_.id,
                plugin_path_,
                "get_batch",
                "expected array",
            });
        }

        std::vector<Chapter> chapters;
        chapters.reserve(result.size());
        for (const auto& item : result) {
            chapters.push_back(parse_chapter(item, info_.id, plugin_path_));
        }
        return chapters;
    });
}

SourceSessionStatus JsBookSource::query_session_status(bool force_refresh) {
    auto status = invoke_with_error_context(info_.id, plugin_path_, "get_status", [&] {
        const auto result = runtime_->call(module_id_, "get_status", json::array({force_refresh}));
        if (result.is_null()) {
            return session_status_;
        }
        if (!result.is_object()) {
            throw SourceException({
                SourceErrorCode::InvalidReturnType,
                info_.id,
                plugin_path_,
                "get_status",
                "expected object",
            });
        }

        SourceSessionStatus parsed;
        parsed.logged_in = logged_in_;
        if (result.contains("logged_in") && !result["logged_in"].is_null()) {
            if (!result["logged_in"].is_boolean()) {
                throw SourceException({
                    SourceErrorCode::InvalidReturnField,
                    info_.id,
                    plugin_path_,
                    "get_status.logged_in",
                    "expected boolean",
                });
            }
            parsed.logged_in = result["logged_in"].get<bool>();
        }

        if (result.contains("remaining_download_quota") && !result["remaining_download_quota"].is_null()) {
            parsed.remaining_download_quota = integer_from_value<int>(
                result["remaining_download_quota"],
                info_.id,
                plugin_path_,
                "get_status.remaining_download_quota",
                0);
        }

        return parsed;
    });

    logged_in_ = status.logged_in;
    if (!logged_in_) {
        status.remaining_download_quota.reset();
    }
    session_status_ = status;
    return session_status_;
}

// ── manifest 解析 ─────────────

/// 从 JSON 对象中解析插件 manifest，填充 SourceInfo 字段
void JsBookSource::load_manifest(const json& manifest) {
    if (!manifest.is_object()) {
        throw SourceException({
            SourceErrorCode::PluginInvalidManifest,
            "",
            plugin_path_,
            "manifest",
            "plugin manifest must be an object",
        });
    }

    info_.id = string_from_value(manifest.value("id", json(nullptr)), "", plugin_path_, "manifest.id", true);
    info_.name = string_from_value(manifest.value("name", json(nullptr)), info_.id, plugin_path_, "manifest.name", true);
    info_.version = string_from_value(manifest.value("version", json(nullptr)), info_.id, plugin_path_, "manifest.version", false);
    info_.author = string_from_value(manifest.value("author", json(nullptr)), info_.id, plugin_path_, "manifest.author", false);
    info_.description = string_from_value(
        manifest.value("description", json(nullptr)),
        info_.id,
        plugin_path_,
        "manifest.description",
        false);
    info_.required_envs = string_list_from_value(
        manifest.value("required_envs", json(nullptr)),
        info_.id,
        plugin_path_,
        "manifest.required_envs");
    info_.optional_envs = string_list_from_value(
        manifest.value("optional_envs", json(nullptr)),
        info_.id,
        plugin_path_,
        "manifest.optional_envs");

    if (info_.id.empty() || info_.name.empty()) {
        throw SourceException({
            SourceErrorCode::PluginInvalidManifest,
            info_.id,
            plugin_path_,
            "manifest",
            "plugin id/name cannot be empty",
        });
    }
}

} // namespace novel
