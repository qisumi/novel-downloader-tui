#pragma once

#include <memory>
#include <optional>
#include <string>

#include "source/domain/book_source.h"
#include "source/js/js_plugin_runtime.h"

namespace novel {

/// 基于 JS 插件的书源实现，将 JS 插件适配到 IBookSource 接口
/// 负责解析 JS 插件返回的 JSON 数据，校验字段类型并转换为 C++ 结构体
class JsBookSource : public IBookSource {
public:
    /// 构造时传入 JS 运行时和引导阶段的插件信息
    JsBookSource(std::shared_ptr<JsPluginRuntime> runtime, const JsBootstrapPlugin& plugin);

    const SourceInfo& info() const override { return info_; }
    const SourceCapabilities& capabilities() const override { return capabilities_; }

    void configure() override;

    std::vector<Book> search(const std::string& keywords, int page) override;
    std::optional<Book> get_book_info(const std::string& book_id) override;
    std::vector<TocItem> get_toc(const std::string& book_id) override;
    std::optional<Chapter> get_chapter(
        const std::string& book_id,
        const std::string& item_id) override;
    bool login() override;
    bool is_logged_in() const override { return logged_in_; }
    int get_batch_count(const std::string& book_id) override;
    std::vector<Chapter> get_batch(const std::string& book_id, int batch_no) override;

private:
    /// 从 JS 插件的 manifest JSON 中解析并填充 SourceInfo
    void load_manifest(const nlohmann::json& manifest);

    std::string                      plugin_path_;  ///< 插件文件绝对路径
    std::string                      module_id_;    ///< 插件模块 ID（如 "fanqie"）
    std::shared_ptr<JsPluginRuntime> runtime_;      ///< JS 插件运行时
    SourceInfo                       info_;         ///< 书源基本信息
    SourceCapabilities               capabilities_; ///< 书源能力声明
    bool                             has_configure_ = false;  ///< 插件是否实现了 configure 方法
    bool                             has_login_ = false;      ///< 插件是否实现了 login 方法
    bool                             has_book_info_ = false;  ///< 插件是否实现了 get_book_info 方法
    bool                             has_batch_count_ = false; ///< 插件是否实现了 get_batch_count 方法
    bool                             has_batch_ = false;       ///< 插件是否实现了 get_batch 方法
    bool                             logged_in_ = false;       ///< 当前会话是否已登录
};

} // namespace novel
