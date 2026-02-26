#include "export/text_sanitizer.h"

#include <cctype>
#include <sstream>

namespace fanqie::text_sanitizer {

static void replace_all(std::string& s,
                        const std::string& from,
                        const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

static std::string decode_html_entities(std::string text) {
    replace_all(text, "&nbsp;", " ");
    replace_all(text, "&ensp;", " ");
    replace_all(text, "&emsp;", " ");
    replace_all(text, "&amp;", "&");
    replace_all(text, "&lt;", "<");
    replace_all(text, "&gt;", ">");
    replace_all(text, "&quot;", "\"");
    replace_all(text, "&apos;", "'");
    replace_all(text, "&#39;", "'");
    replace_all(text, "&#34;", "\"");
    replace_all(text, "&#60;", "<");
    replace_all(text, "&#62;", ">");
    replace_all(text, "&#38;", "&");
    return text;
}

static bool iequals_ascii(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

static std::string strip_html_tags(std::string text) {
    std::string out;
    out.reserve(text.size());

    const auto append_newline_for_block_tag = [&](const std::string& tag_name) {
        static const char* kBlockTags[] = {
            "p", "div", "br", "li", "section", "article", "footer", "header", "h1", "h2", "h3"
        };
        for (auto* t : kBlockTags) {
            if (iequals_ascii(tag_name, t)) {
                if (out.empty() || out.back() != '\n') out.push_back('\n');
                return;
            }
        }
    };

    for (size_t i = 0; i < text.size();) {
        if (text[i] != '<') {
            out.push_back(text[i]);
            ++i;
            continue;
        }

        const size_t close = text.find('>', i + 1);
        if (close == std::string::npos) {
            out.push_back(text[i]);
            ++i;
            continue;
        }

        std::string tag = text.substr(i + 1, close - (i + 1));
        size_t start = 0;
        while (start < tag.size() && std::isspace(static_cast<unsigned char>(tag[start]))) ++start;
        if (start < tag.size() && tag[start] == '/') ++start;
        size_t end = start;
        while (end < tag.size() &&
               (std::isalnum(static_cast<unsigned char>(tag[end])) || tag[end] == '_' || tag[end] == '-')) {
            ++end;
        }
        std::string tag_name = tag.substr(start, end - start);
        append_newline_for_block_tag(tag_name);
        i = close + 1;
    }

    return out;
}

static std::string normalize_whitespace(std::string text) {
    replace_all(text, "\r\n", "\n");
    replace_all(text, "\r", "\n");

    std::string out;
    out.reserve(text.size());

    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        out += line;
        out += "\n";
    }

    return out;
}

std::string sanitize_filename(std::string name) {
    for (char& c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return name;
}

std::string html_to_plain_text(const std::string& input) {
    auto s = decode_html_entities(input);
    s = strip_html_tags(std::move(s));
    s = normalize_whitespace(std::move(s));
    return s;
}

} // namespace fanqie::text_sanitizer
