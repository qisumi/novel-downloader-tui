#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <webview/webview.h>

namespace novel {

class HostApi;

struct JsModule {
    std::string module_id;
    std::string plugin_path;
    std::string source;
    bool        is_plugin_candidate = false;
};

struct JsBootstrapPlugin {
    std::string    module_id;
    std::string    plugin_path;
    nlohmann::json manifest;
    bool           has_configure = false;
    bool           has_search = false;
    bool           has_book_info = false;
    bool           has_toc = false;
    bool           has_chapter = false;
};

class JsPluginRuntime {
public:
    using json = nlohmann::json;

    JsPluginRuntime(webview::webview& window, std::shared_ptr<HostApi> host_api);

    void install();
    void queue_modules(const std::vector<JsModule>& modules);
    void queue_bootstrap(const std::vector<JsModule>& plugins);

    std::vector<JsBootstrapPlugin> wait_for_bootstrap();
    json call(const std::string& module_id, const std::string& operation, const json& args);

private:
    struct PendingCall {
        bool                      completed = false;
        std::optional<json>       result;
        std::optional<std::string> error;
    };

    webview::webview&                 window_;
    std::shared_ptr<HostApi>          host_api_;
    std::mutex                        bootstrap_mutex_;
    std::condition_variable           bootstrap_cv_;
    bool                              bootstrap_finished_ = false;
    bool                              bootstrap_failed_ = false;
    std::string                       bootstrap_error_;
    std::vector<JsBootstrapPlugin>    bootstrap_plugins_;

    std::mutex                                 calls_mutex_;
    std::condition_variable                    calls_cv_;
    std::uint64_t                              next_call_id_ = 1;
    std::unordered_map<std::string, PendingCall> pending_calls_;

    void bind_async(const std::string& name, std::function<json(const json&)> handler);

    static json parse_args(const std::string& raw_args);
};

} // namespace novel
