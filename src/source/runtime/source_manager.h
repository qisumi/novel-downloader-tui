#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "source/domain/book_source.h"

namespace novel {

class JsPluginRuntime;

/// 书源管理器，负责插件扫描、加载、书源切换等生命周期管理
/// 内部协调 JsPluginRuntime 和 JsBookSource，对外提供统一的书源访问接口
class SourceManager {
public:
    explicit SourceManager(std::shared_ptr<JsPluginRuntime> plugin_runtime);

    /// 从指定目录扫描并加载 JS 插件
    /// 会递归遍历目录下所有 .js 文件，区分普通模块和书源候选插件
    void load_from_directory(const std::string& plugin_dir);

    /// 设置首选书源 ID，初始化完成后会自动切换到该书源
    void set_preferred_source(const std::string& source_id);

    /// 列出所有已加载书源的基本信息
    std::vector<SourceInfo> list_sources() const;

    /// 切换当前书源，返回是否切换成功
    bool select_source(const std::string& source_id);

    /// 获取当前书源实例（若未初始化会阻塞等待）
    std::shared_ptr<IBookSource> current_source() const;

    /// 获取当前书源的基本信息
    std::optional<SourceInfo> current_info() const;

    /// 对当前书源执行 configure 操作
    void configure_current();

private:
    /// 确保插件引导流程已完成（懒初始化，首次访问时阻塞等待）
    void ensure_ready() const;

    /// 不加锁版本的书源切换（调用方需持有 mutex_）
    bool select_source_unlocked(const std::string& source_id) const;

    std::shared_ptr<JsPluginRuntime>             plugin_runtime_;      ///< JS 插件运行时
    std::string                                  plugin_dir_;          ///< 插件目录路径
    mutable std::mutex                           mutex_;               ///< 状态互斥锁
    mutable bool                                 initialized_ = false; ///< 是否已完成引导初始化
    std::string                                  preferred_source_id_; ///< 首选书源 ID
    mutable std::vector<std::shared_ptr<IBookSource>> sources_;        ///< 已加载的书源列表
    mutable std::shared_ptr<IBookSource>         current_source_;      ///< 当前激活的书源
};

} // namespace novel
