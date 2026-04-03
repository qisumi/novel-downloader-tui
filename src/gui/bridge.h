/// @file bridge.h
/// @brief C++ <-> JavaScript 桥接层
///
/// 将 C++ 服务能力通过 WebView bind 机制暴露为 JavaScript API（window.app.*）。
/// 前端通过 Promise 风格调用，C++ 端在后台线程处理并通过事件推送进度更新。
///
/// 暴露的 API 列表：
///   - get_sources      获取所有书源列表
///   - select_source    切换当前书源
///   - search_books     搜索书籍
///   - get_book_detail  获取书籍详情
///   - get_toc          获取目录
///   - download_book    下载书籍
///   - login            执行当前书源登录（可选）
///   - getSourceCapabilities 获取当前书源能力
///   - export_book      导出书籍（EPUB/TXT）
///   - list_bookshelf   列出书架
///   - save_bookshelf   保存到书架
///   - remove_bookshelf 从书架移除

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

/// C++ <-> JS 桥接器
///
/// 负责将 C++ 服务能力注册为 WebView JS 绑定，
/// 处理参数解析、异步执行、错误捕获和结果回传。
class GuiBridge {
public:
    /// 构造桥接器
    /// @param window  WebView 实例
    /// @param runtime 应用运行时（提供各服务访问）
    GuiBridge(webview::webview& window, GuiAppRuntime& runtime);

    /// 安装所有 JS 绑定和初始化脚本
    ///
    /// 注入 window.__NOVEL_BRIDGE__ 和 window.app 命名空间，
    /// 注册所有 native_* 绑定函数
    void install();

private:
    using json = nlohmann::json;

    /// 参数校验异常，用于在 handler 内抛出以返回验证错误
    struct ValidationError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    webview::webview&  window_;        ///< WebView 实例引用
    GuiAppRuntime&     runtime_;       ///< 应用运行时引用
    std::mutex         core_mutex_;    ///< 保护书源操作的互斥锁
    std::atomic_uint64_t next_task_id_{1};  ///< 任务 ID 计数器

    /// 注册异步 JS 绑定函数
    ///
    /// handler 在后台线程执行，自动捕获异常并返回标准化的成功/错误响应
    void bind_async(
        const std::string& name,
        std::function<json(const json&)> handler);

    /// 向 JS 端返回成功结果
    void resolve_success(const std::string& call_id, json payload);
    /// 向 JS 端返回错误结果
    void resolve_error(const std::string& call_id, const json& payload);
    /// 向前端派发自定义事件（window.dispatchEvent(new CustomEvent("novel:name", ...))）
    void emit_event(const std::string& name, const json& payload);
    /// 解析 JS 端传入的原始参数字符串为 JSON 数组
    json parse_args(const std::string& raw_args);

    /// 构造标准成功响应 { ok: true, data: ... }
    static json make_success(json data);
    /// 构造标准错误响应 { ok: false, error: { type, code, message, details } }
    static json make_error(
        std::string type,
        std::string code,
        std::string message,
        json details = json::object());

    /// 从参数数组中获取必需的字符串参数
    static std::string require_string_arg(
        const json& args,
        std::size_t index,
        const char* field_name);
    /// 从参数数组中获取可选的整数参数
    static int optional_int_arg(
        const json& args,
        std::size_t index,
        int default_value);
    /// 从参数数组中获取可选的布尔参数
    static bool optional_bool_arg(
        const json& args,
        std::size_t index,
        bool default_value);
    /// 从参数数组中获取可选的对象参数
    static json optional_object_arg(const json& args, std::size_t index);
};

} // namespace novel
