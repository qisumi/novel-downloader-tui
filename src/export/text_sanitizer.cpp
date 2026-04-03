/// @file text_sanitizer.cpp
/// @brief 文本清洗工具集实现
///
/// 包含 HTML 实体解码、HTML 标签剥离（识别块级元素自动插入换行）、
/// 空白行规范化以及文件名安全化处理的实现。

#include "export/text_sanitizer.h"

#include <cctype>
#include <sstream>

namespace novel::text_sanitizer {

/// 全局字符串替换，将 s 中所有 from 替换为 to
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

/// 解码常见 HTML 命名实体和数字实体
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

/// ASCII 大小写不敏感字符串比较
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

/// 剥离 HTML 标签，对块级元素自动插入换行符
///
/// 识别的块级标签：p, div, br, li, section, article, footer, header, h1, h2, h3
/// 其他标签直接移除，不插入额外空白。
static std::string strip_html_tags(std::string text) {
    std::string out;
    out.reserve(text.size());

    // 判断当前标签是否为块级元素，若是则追加一个换行
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
        // 非标签字符直接追加
        if (text[i] != '<') {
            out.push_back(text[i]);
            ++i;
            continue;
        }

        // 查找标签闭合位置
        const size_t close = text.find('>', i + 1);
        if (close == std::string::npos) {
            out.push_back(text[i]);
            ++i;
            continue;
        }

        // 提取标签内部文本，跳过空白和可选的 '/'
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

/// 规范化空白字符：统一换行符、去除行尾空白、删除空行
static std::string normalize_whitespace(std::string text) {
    // 统一换行符为 \n
    replace_all(text, "\r\n", "\n");
    replace_all(text, "\r", "\n");

    std::string out;
    out.reserve(text.size());

    // 逐行处理：去除行尾空白，跳过空行
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

/// 替换 Windows 文件名中的非法字符为下划线
std::string sanitize_filename(std::string name) {
    for (char& c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    return name;
}

/// HTML 富文本转纯文本的主入口
///
/// 执行顺序：HTML 实体解码 -> 标签剥离 -> 空白规范化
std::string html_to_plain_text(const std::string& input) {
    auto s = decode_html_entities(input);
    s = strip_html_tags(std::move(s));
    s = normalize_whitespace(std::move(s));
    return s;
}

} // namespace novel::text_sanitizer
