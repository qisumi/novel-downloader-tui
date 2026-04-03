#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "source/domain/book_source.h"

namespace novel {

class JsPluginRuntime;

class SourceManager {
public:
    explicit SourceManager(std::shared_ptr<JsPluginRuntime> plugin_runtime);

    void load_from_directory(const std::string& plugin_dir);
    void set_preferred_source(const std::string& source_id);
    std::vector<SourceInfo> list_sources() const;
    bool select_source(const std::string& source_id);
    std::shared_ptr<IBookSource> current_source() const;
    std::optional<SourceInfo> current_info() const;
    void configure_current();

private:
    void ensure_ready() const;
    bool select_source_unlocked(const std::string& source_id) const;

    std::shared_ptr<JsPluginRuntime>             plugin_runtime_;
    std::string                                  plugin_dir_;
    mutable std::mutex                           mutex_;
    mutable bool                                 initialized_ = false;
    std::string                                  preferred_source_id_;
    mutable std::vector<std::shared_ptr<IBookSource>> sources_;
    mutable std::shared_ptr<IBookSource>         current_source_;
};

} // namespace novel
