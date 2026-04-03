/// @file app_runtime.h
/// @brief GUI 应用运行时
///
/// 管理整个 GUI 应用的生命周期和各核心组件实例：
///   - 路径解析（GuiPaths）
///   - HTTP 服务与宿主 API
///   - JS 插件运行时
///   - 书源管理器
///   - SQLite 数据库
///   - 书架 / 下载 / 导出服务

#pragma once

#include <memory>

#include "application/download_service.h"
#include "application/export_service.h"
#include "application/library_service.h"
#include "db/database.h"
#include "gui/user_paths.h"
#include "source/host/host_api.h"
#include "source/host/http_service.h"
#include "source/js/js_plugin_runtime.h"
#include "source/runtime/source_manager.h"
#include <webview/webview.h>

namespace novel {

/// GUI 应用运行时
///
/// 持有所有核心组件的 shared_ptr，提供统一的初始化入口和访问接口。
/// 由 main.cpp 创建，GuiBridge 通过引用访问各服务。
class GuiAppRuntime {
public:
    /// 初始化所有核心组件
    ///
    /// 执行顺序：解析路径 -> 加载 .env -> 初始化日志 -> 创建各服务 -> 扫描插件
    /// @param window WebView 实例，JS 插件运行时需要在其上注入脚本
    void initialize(webview::webview& window);

    /// 导航 WebView 到前端入口页面
    void navigate_frontend(webview::webview& window) const;

    /// 获取运行时路径信息
    const GuiPaths& paths() const { return paths_; }
    /// 获取书源管理器
    const std::shared_ptr<SourceManager>& source_manager() const { return source_manager_; }
    /// 获取书架管理服务
    const std::shared_ptr<LibraryService>& library_service() const { return library_service_; }
    /// 获取下载服务
    const std::shared_ptr<DownloadService>& download_service() const { return download_service_; }
    /// 获取导出服务
    const std::shared_ptr<ExportService>& export_service() const { return export_service_; }
    /// 获取数据库实例
    const std::shared_ptr<Database>& database() const { return database_; }

private:
    GuiPaths                       paths_;             ///< 运行时路径
    std::shared_ptr<HttpService>   http_service_;      ///< HTTP 请求服务
    std::shared_ptr<HostApi>       host_api_;           ///< 插件宿主 API
    std::shared_ptr<JsPluginRuntime> plugin_runtime_;   ///< JS 插件运行时
    std::shared_ptr<SourceManager> source_manager_;     ///< 书源管理器
    std::shared_ptr<Database>      database_;           ///< SQLite 数据库
    std::shared_ptr<LibraryService> library_service_;   ///< 书架管理服务
    std::shared_ptr<DownloadService> download_service_;  ///< 下载服务
    std::shared_ptr<ExportService>   export_service_;    ///< 导出服务
};

} // namespace novel
