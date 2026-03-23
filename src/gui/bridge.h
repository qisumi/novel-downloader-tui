#pragma once

#include <atomic>
#include <mutex>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>
#include <webview/webview.h>

#include "gui/app_runtime.h"
#include "source/domain/source_errors.h"

namespace novel {

class GuiBridge {
public:
    GuiBridge(webview::webview& window, GuiAppRuntime& runtime);

    void install();

private:
    using json = nlohmann::json;

    struct ValidationError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    webview::webview&  window_;
    GuiAppRuntime&     runtime_;
    std::mutex         core_mutex_;
    std::atomic_uint64_t next_task_id_{1};

    void bind_async(
        const std::string& name,
        std::function<json(const json&)> handler);

    void resolve_success(const std::string& call_id, json payload);
    void resolve_error(const std::string& call_id, const json& payload);
    void emit_event(const std::string& name, const json& payload);
    json parse_args(const std::string& raw_args);

    static json make_success(json data);
    static json make_error(
        std::string type,
        std::string code,
        std::string message,
        json details = json::object());

    static std::string require_string_arg(
        const json& args,
        std::size_t index,
        const char* field_name);
    static int optional_int_arg(
        const json& args,
        std::size_t index,
        int default_value);
    static bool optional_bool_arg(
        const json& args,
        std::size_t index,
        bool default_value);
    static json optional_object_arg(const json& args, std::size_t index);
};

} // namespace novel
