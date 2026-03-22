#pragma once

#include <string>

namespace novel::text_sanitizer {

std::string sanitize_filename(std::string name);
std::string html_to_plain_text(const std::string& input);

} // namespace novel::text_sanitizer
