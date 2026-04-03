#pragma once

#include <memory>
#include <optional>
#include <string>

#include "source/domain/book_source.h"
#include "source/js/js_plugin_runtime.h"

namespace novel {

class JsBookSource : public IBookSource {
public:
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

private:
    void load_manifest(const nlohmann::json& manifest);

    std::string                      plugin_path_;
    std::string                      module_id_;
    std::shared_ptr<JsPluginRuntime> runtime_;
    SourceInfo                       info_;
    SourceCapabilities               capabilities_;
    bool                             has_configure_ = false;
    bool                             has_book_info_ = false;
};

} // namespace novel
