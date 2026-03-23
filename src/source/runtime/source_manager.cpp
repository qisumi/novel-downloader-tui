#include "source/runtime/source_manager.h"

#include <algorithm>
#include <filesystem>

#include <spdlog/spdlog.h>

#include "source/domain/source_errors.h"
#include "source/host/host_api.h"
#include "source/lua/lua_book_source.h"
#include "source/lua/lua_runtime.h"

namespace novel {

SourceManager::SourceManager(std::shared_ptr<HostApi> host_api)
    : host_api_(std::move(host_api)) {}

void SourceManager::load_from_directory(const std::string& plugin_dir) {
    namespace fs = std::filesystem;

    sources_.clear();
    current_source_.reset();

    const fs::path plugin_path(plugin_dir);
    const fs::path cwd = fs::current_path();
    spdlog::info("Plugin load start. cwd='{}', plugin_dir='{}'", cwd.string(), plugin_path.string());
    if (plugin_path.is_relative()) {
        spdlog::info("Plugin dir is relative. resolved='{}'", fs::absolute(plugin_path).string());
    }

    if (!fs::exists(plugin_path)) {
        spdlog::error("Plugin directory does not exist: {}", plugin_path.string());
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "plugin directory does not exist"});
    }
    if (!fs::is_directory(plugin_path)) {
        spdlog::error("Plugin path is not a directory: {}", plugin_path.string());
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "plugin path is not a directory"});
    }

    std::vector<fs::path> files;
    for (const auto& entry : fs::directory_iterator(plugin_path)) {
        if (!entry.is_regular_file()) {
            spdlog::debug("Skip non-file entry: {}", entry.path().string());
            continue;
        }
        if (entry.path().extension() != ".lua") {
            spdlog::debug("Skip non-lua file: {}", entry.path().string());
            continue;
        }
        if (entry.path().filename().string().starts_with("_")) {
            spdlog::debug("Skip private lua file: {}", entry.path().string());
            continue;
        }
        spdlog::info("Discovered plugin candidate: {}", entry.path().string());
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    spdlog::info("Plugin scan finished. candidate_count={}", files.size());

    for (const auto& path : files) {
        try {
            spdlog::info("Loading plugin file: {}", path.string());
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
        } catch (const SourceException& e) {
            spdlog::error("Failed to load plugin {}: {}", path.string(),
                          format_source_error_log(e.error()));
        } catch (const std::exception& e) {
            spdlog::error("Failed to load plugin {}: {}", path.string(), e.what());
        }
    }

    if (sources_.empty()) {
        spdlog::error("No valid source plugins found in {}", plugin_path.string());
        throw SourceException({SourceErrorCode::PluginLoadFailed, "", plugin_dir,
                               "scan", "no valid source plugins found"});
    }

    current_source_ = sources_.front();
    spdlog::info("Plugin load complete. loaded_count={}, current_source='{}'",
                 sources_.size(), current_source_->info().id);
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

} // namespace novel
