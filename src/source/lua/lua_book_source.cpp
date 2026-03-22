#include "source/lua/lua_book_source.h"

#include "source/domain/source_errors.h"
#include "source/lua/lua_runtime.h"
#include "source/lua/lua_source_schema.h"

namespace fanqie {

namespace {

constexpr std::string_view k_config_error_prefix = "__fanqie_config_error__:";

[[nodiscard]] bool is_config_error(std::string_view message) {
    return message.starts_with(k_config_error_prefix);
}

[[nodiscard]] std::string strip_config_error_prefix(std::string_view message) {
    if (!is_config_error(message)) {
        return std::string(message);
    }
    return std::string(message.substr(k_config_error_prefix.size()));
}

template <typename T>
T cast_or_throw(
    const luabridge::LuaRef& value,
    const std::string& source_id,
    const std::string& field,
    const std::string& plugin_path) {
    auto result = value.cast<T>();
    if (!result) {
        throw SourceException({SourceErrorCode::InvalidReturnField, source_id, plugin_path,
                               field, "invalid manifest field: " + field});
    }
    return result.value();
}

luabridge::LuaRef call_to_ref(
    const luabridge::LuaRef& fn,
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& operation) {
    auto result = luabridge::call(fn);
    if (!result) {
        throw SourceException({SourceErrorCode::PluginRuntimeError, source_id, plugin_path,
                               operation, result.errorMessage()});
    }
    return result.size() > 0 ? result[0] : luabridge::LuaRef(fn.state());
}

template <typename... Args>
luabridge::LuaRef call_to_ref(
    const luabridge::LuaRef& fn,
    const std::string& source_id,
    const std::string& plugin_path,
    const std::string& operation,
    Args&&... args) {
    auto result = luabridge::call(fn, std::forward<Args>(args)...);
    if (!result) {
        const auto error_message = result.errorMessage();
        const auto error_code = is_config_error(error_message)
            ? SourceErrorCode::PluginConfigError
            : SourceErrorCode::PluginRuntimeError;
        throw SourceException({error_code, source_id, plugin_path, operation,
                               strip_config_error_prefix(error_message)});
    }
    return result.size() > 0 ? result[0] : luabridge::LuaRef(fn.state());
}

} // namespace

LuaBookSource::LuaBookSource(
    std::string plugin_path,
    std::shared_ptr<LuaRuntime> runtime,
    luabridge::LuaRef plugin_ref)
    : plugin_path_(std::move(plugin_path)),
      runtime_(std::move(runtime)),
      plugin_ref_(std::move(plugin_ref)) {
    load_manifest();
}

void LuaBookSource::load_manifest() {
    luabridge::LuaRef manifest = plugin_ref_["manifest"];
    if (!manifest.isTable()) {
        throw SourceException({SourceErrorCode::PluginInvalidManifest, "", plugin_path_,
                               "manifest", "plugin manifest must be a table"});
    }

    info_.id = cast_or_throw<std::string>(manifest["id"], "", "manifest.id", plugin_path_);
    info_.name = cast_or_throw<std::string>(manifest["name"], info_.id, "manifest.name", plugin_path_);
    info_.version = manifest["version"].isNil()
        ? ""
        : cast_or_throw<std::string>(manifest["version"], info_.id, "manifest.version", plugin_path_);
    info_.author = manifest["author"].isNil()
        ? ""
        : cast_or_throw<std::string>(manifest["author"], info_.id, "manifest.author", plugin_path_);
    info_.description = manifest["description"].isNil()
        ? ""
        : cast_or_throw<std::string>(manifest["description"], info_.id, "manifest.description", plugin_path_);

    if (info_.id.empty() || info_.name.empty()) {
        throw SourceException({SourceErrorCode::PluginInvalidManifest, "", plugin_path_,
                               "manifest", "manifest.id and manifest.name are required"});
    }

    require_function("search");
    require_function("get_toc");
    require_function("get_chapter");
}

luabridge::LuaRef LuaBookSource::require_function(const char* name) {
    luabridge::LuaRef fn = plugin_ref_[name];
    if (!fn.isFunction()) {
        throw SourceException({SourceErrorCode::PluginMissingMethod, info_.id, plugin_path_,
                               name, "missing required plugin function"});
    }
    return fn;
}

void LuaBookSource::configure() {
    std::lock_guard lock(mutex_);
    luabridge::LuaRef fn = plugin_ref_["configure"];
    if (fn.isFunction()) {
        call_to_ref(fn, info_.id, plugin_path_, "configure");
    }
}

std::vector<Book> LuaBookSource::search(const std::string& keywords, int page) {
    std::lock_guard lock(mutex_);
    try {
        return parse_book_list(
            call_to_ref(require_function("search"), info_.id, plugin_path_, "search", keywords, page),
            info_.id);
    } catch (const luabridge::LuaException& e) {
        const auto error_message = std::string(e.what());
        const auto error_code = is_config_error(error_message)
            ? SourceErrorCode::PluginConfigError
            : SourceErrorCode::PluginRuntimeError;
        throw SourceException({error_code, info_.id, plugin_path_, "search",
                               strip_config_error_prefix(error_message)});
    }
}

std::optional<Book> LuaBookSource::get_book_info(const std::string& book_id) {
    std::lock_guard lock(mutex_);
    luabridge::LuaRef fn = plugin_ref_["get_book_info"];
    if (!fn.isFunction()) {
        return std::nullopt;
    }
    try {
        return parse_optional_book(
            call_to_ref(fn, info_.id, plugin_path_, "get_book_info", book_id), info_.id);
    } catch (const luabridge::LuaException& e) {
        const auto error_message = std::string(e.what());
        const auto error_code = is_config_error(error_message)
            ? SourceErrorCode::PluginConfigError
            : SourceErrorCode::PluginRuntimeError;
        throw SourceException({error_code, info_.id, plugin_path_, "get_book_info",
                               strip_config_error_prefix(error_message)});
    }
}

std::vector<TocItem> LuaBookSource::get_toc(const std::string& book_id) {
    std::lock_guard lock(mutex_);
    try {
        return parse_toc_list(
            call_to_ref(require_function("get_toc"), info_.id, plugin_path_, "get_toc", book_id),
            info_.id);
    } catch (const luabridge::LuaException& e) {
        const auto error_message = std::string(e.what());
        const auto error_code = is_config_error(error_message)
            ? SourceErrorCode::PluginConfigError
            : SourceErrorCode::PluginRuntimeError;
        throw SourceException({error_code, info_.id, plugin_path_, "get_toc",
                               strip_config_error_prefix(error_message)});
    }
}

std::optional<Chapter> LuaBookSource::get_chapter(
    const std::string& book_id,
    const std::string& item_id) {
    std::lock_guard lock(mutex_);
    try {
        auto chapter = parse_optional_chapter(
            call_to_ref(require_function("get_chapter"), info_.id, plugin_path_,
                        "get_chapter", book_id, item_id),
            info_.id);
        if (chapter && chapter->item_id.empty()) {
            chapter->item_id = item_id;
        }
        return chapter;
    } catch (const luabridge::LuaException& e) {
        const auto error_message = std::string(e.what());
        const auto error_code = is_config_error(error_message)
            ? SourceErrorCode::PluginConfigError
            : SourceErrorCode::PluginRuntimeError;
        throw SourceException({error_code, info_.id, plugin_path_, "get_chapter",
                               strip_config_error_prefix(error_message)});
    }
}

} // namespace fanqie
