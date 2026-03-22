#pragma once

#include <string>
#include <vector>

namespace fanqie {

struct SourceInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::vector<std::string> required_envs;
    std::vector<std::string> optional_envs;
};

struct SourceCapabilities {
    bool supports_search = true;
    bool supports_book_info = true;
    bool supports_toc = true;
    bool supports_chapter = true;
};

} // namespace fanqie
