#pragma once

#include <string>

namespace fanqie {

struct SourceInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
};

struct SourceCapabilities {
    bool supports_search = true;
    bool supports_book_info = true;
    bool supports_toc = true;
    bool supports_chapter = true;
};

} // namespace fanqie
