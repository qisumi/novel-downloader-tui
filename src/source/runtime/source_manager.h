#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "source/domain/book_source.h"

namespace fanqie {

class HostApi;

class SourceManager {
public:
    explicit SourceManager(std::shared_ptr<HostApi> host_api);

    void load_from_directory(const std::string& plugin_dir);
    std::vector<SourceInfo> list_sources() const;
    bool select_source(const std::string& source_id);
    std::shared_ptr<IBookSource> current_source() const;
    std::optional<SourceInfo> current_info() const;
    void configure_current(const SourceContext& context);

private:
    std::shared_ptr<HostApi>                   host_api_;
    std::vector<std::shared_ptr<IBookSource>> sources_;
    std::shared_ptr<IBookSource>              current_source_;
};

} // namespace fanqie
