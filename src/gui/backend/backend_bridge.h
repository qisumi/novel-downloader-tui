#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace novel::gui {

struct BackendRequest {
    std::string db_path = "novel.db";
    std::string output_dir = ".";
    std::string plugin_dir = "plugins";
    std::string source_id;

    std::string command;
    std::string keywords;
    int         page = 0;
    std::string book_json;
    std::string book_id;
    bool        force_remote = false;
    int         start = 0;
    int         end = -1;
    std::string format = "epub";
};

void           initialize_backend_runtime();
BackendRequest request_from_json(const nlohmann::json& j);
nlohmann::json request_to_json(const BackendRequest& request);
nlohmann::json execute_backend_request(const BackendRequest& request);
std::string    execute_backend_request_json(const std::string& request_json);

} // namespace novel::gui
