#include <cstdlib>
#include <cstring>
#include <string>

#include "gui/backend/backend_bridge.h"

#ifdef _WIN32
#define NOVEL_GUI_EXPORT extern "C" __declspec(dllexport)
#else
#define NOVEL_GUI_EXPORT extern "C"
#endif

namespace {

char* copy_to_c_string(const std::string& text) {
    auto* buffer = static_cast<char*>(std::malloc(text.size() + 1));
    if (buffer == nullptr) {
        return nullptr;
    }

    std::memcpy(buffer, text.data(), text.size());
    buffer[text.size()] = '\0';
    return buffer;
}

} // namespace

NOVEL_GUI_EXPORT const char* novel_gui_invoke(const char* request_json) {
    const std::string request = request_json == nullptr ? std::string("{}") : std::string(request_json);
    const auto        response = novel::gui::execute_backend_request_json(request);
    return copy_to_c_string(response);
}

NOVEL_GUI_EXPORT void novel_gui_free(const char* text) {
    std::free(const_cast<char*>(text));
}
