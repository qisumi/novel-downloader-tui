#pragma once

#include <lua.hpp>
#include <luabridge3/LuaBridge/LuaBridge.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "source/domain/book_source.h"

namespace fanqie {

class LuaRuntime;

class LuaBookSource : public IBookSource {
public:
    LuaBookSource(
        std::string plugin_path,
        std::shared_ptr<LuaRuntime> runtime,
        luabridge::LuaRef plugin_ref);

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
    void load_manifest();
    luabridge::LuaRef require_function(const char* name);

    std::string                 plugin_path_;
    std::shared_ptr<LuaRuntime> runtime_;
    luabridge::LuaRef           plugin_ref_;
    SourceInfo                  info_;
    SourceCapabilities          capabilities_;
    std::mutex                  mutex_;
};

} // namespace fanqie
