#include "source/runtime/source_manager.h"

#include <algorithm>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "source/domain/source_errors.h"
#include "source/host/host_api.h"
#include "source/lua/lua_book_source.h"
#include "source/lua/lua_runtime.h"

namespace fanqie {

SourceManager::SourceManager(std::shared_ptr<HostApi> host_api)
    : host_api_(std::move(host_api)) {}

void SourceManager::load_from_directory(const std::string& plugin_dir) {
    namespace fs = std::filesystem;

    sources_.clear();
    current_source_.reset();

    if (!fs::exists(plugin_dir)) {
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "plugin directory does not exist"});
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(plugin_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".lua") {
            continue;
        }
        if (entry.path().filename().string().starts_with("_")) {
            continue;
        }
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& path : files) {
        try {
            auto runtime = std::make_shared<LuaRuntime>(host_api_);
            auto plugin_ref = runtime->load_plugin(path.string());
            auto source = std::make_shared<LuaBookSource>(path.string(), runtime, plugin_ref);

            auto duplicate = std::find_if(
                sources_.begin(), sources_.end(),
                [&](const std::shared_ptr<IBookSource>& current) {
                    return current->info().id == source->info().id;
                });
            if (duplicate != sources_.end()) {
                spdlog::error("duplicate source id '{}', ignoring {}",
                              source->info().id, path.string());
                continue;
            }

            spdlog::info("Loaded source '{}' from {}", source->info().id, path.string());
            sources_.push_back(std::move(source));
        } catch (const std::exception& e) {
            spdlog::error("Failed to load plugin {}: {}", path.string(), e.what());
        }
    }

    if (sources_.empty()) {
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "no valid source plugins found"});
    }

    current_source_ = sources_.front();
}

std::vector<SourceInfo> SourceManager::list_sources() const {
    std::vector<SourceInfo> infos;
    infos.reserve(sources_.size());
    for (const auto& source : sources_) {
        infos.push_back(source->info());
    }
    return infos;
}

bool SourceManager::select_source(const std::string& source_id) {
    auto it = std::find_if(
        sources_.begin(), sources_.end(),
        [&](const std::shared_ptr<IBookSource>& source) {
            return source->info().id == source_id;
        });
    if (it == sources_.end()) {
        return false;
    }
    current_source_ = *it;
    return true;
}

std::shared_ptr<IBookSource> SourceManager::current_source() const {
    if (!current_source_) {
        throw SourceException({SourceErrorCode::SourceNotSelected, "", "", "current_source",
                               "no source selected"});
    }
    return current_source_;
}

std::optional<SourceInfo> SourceManager::current_info() const {
    if (!current_source_) {
        return std::nullopt;
    }
    return current_source_->info();
}

void SourceManager::configure_current() {
    current_source()->configure();
}

} // namespace fanqie
