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

/// JS 模块描述，对应一个 .js 文件（含模块 ID、文件路径、源码内容）
struct JsModule {
    std::string module_id;              ///< 模块 ID（如 "fanqie" 或 "_shared/common"）
    std::string plugin_path;            ///< 文件绝对路径
    std::string source;                 ///< 文件源码内容
    bool        is_plugin_candidate = false; ///< 是否为书源候选插件（路径段不以 _ 开头）
};

/// JS 引导阶段的插件信息，由 JS 运行时加载插件后回传
struct JsBootstrapPlugin {
    std::string    module_id;       ///< 模块 ID
    std::string    plugin_path;     ///< 文件绝对路径
    nlohmann::json manifest;        ///< 插件 manifest 对象
    bool           has_configure = false;  ///< 是否导出 configure 方法
    bool           has_search = false;     ///< 是否导出 search 方法
    bool           has_book_info = false;  ///< 是否导出 get_book_info 方法
    bool           has_toc = false;        ///< 是否导出 get_toc 方法
    bool           has_chapter = false;    ///< 是否导出 get_chapter 方法
};

/// JS 插件运行时，在 WebView 中管理 JS 模块的注册、加载和调用
/// 提供 CommonJS 风格的 require()、host API 桥接、异步 RPC 调用机制
class JsPluginRuntime {
public:
    using json = nlohmann::json;

    /// 构造时传入 WebView 窗口引用和宿主 API
    JsPluginRuntime(webview::webview& window, std::shared_ptr<HostApi> host_api);

    /// 向 WebView 注入 JS 插件运行时框架（含 host 对象、require 机制等）
    void install();

    /// 将 JS 模块注册到 WebView 中的模块表
    void queue_modules(const std::vector<JsModule>& modules);

    /// 触发引导流程：依次加载候选插件、检测导出方法、收集 manifest
    void queue_bootstrap(const std::vector<JsModule>& plugins);

    /// 阻塞等待引导流程完成（超时 30 秒），返回成功加载的插件列表
    std::vector<JsBootstrapPlugin> wait_for_bootstrap();

    /// 调用指定插件的某个方法（异步 RPC，阻塞等待结果，超时 60 秒）
    /// @param module_id  模块 ID
    /// @param operation  方法名（如 "search"、"get_chapter"）
    /// @param args       参数列表（JSON 数组）
    json call(const std::string& module_id, const std::string& operation, const json& args);

private:
    /// 一次挂起的 RPC 调用状态
    struct PendingCall {
        bool                      completed = false; ///< 是否已完成
        std::optional<json>       result;            ///< 成功时的返回值
        std::optional<std::string> error;            ///< 失败时的错误消息
    };

    webview::webview&                 window_;       ///< WebView 窗口引用
    std::shared_ptr<HostApi>          host_api_;     ///< 宿主 API（HTTP / env / log）

    // ── 引导阶段状态 ─────────────
    std::mutex                        bootstrap_mutex_;       ///< 引导状态互斥锁
    std::condition_variable           bootstrap_cv_;          ///< 引导完成条件变量
    bool                              bootstrap_finished_ = false; ///< 引导是否完成
    bool                              bootstrap_failed_ = false;   ///< 引导是否失败
    std::string                       bootstrap_error_;       ///< 引导失败原因
    std::vector<JsBootstrapPlugin>    bootstrap_plugins_;     ///< 引导成功加载的插件列表

    // ── RPC 调用状态 ─────────────
    std::mutex                                 calls_mutex_;    ///< 调用状态互斥锁
    std::condition_variable                    calls_cv_;       ///< 调用完成条件变量
    std::uint64_t                              next_call_id_ = 1; ///< 下一个调用 ID
    std::unordered_map<std::string, PendingCall> pending_calls_; ///< 挂起的调用映射表

    /// 以异步线程方式绑定 WebView JS 回调，避免阻塞 WebView 主线程
    void bind_async(const std::string& name, std::function<json(const json&)> handler);

    /// 解析 WebView 传来的原始 JSON 字符串参数为 JSON 数组
    static json parse_args(const std::string& raw_args);
};

} // namespace novel
