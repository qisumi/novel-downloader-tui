/// @file text_sanitizer.h
/// @brief 文本清洗工具集
///
/// 提供 HTML 标签剥离、HTML 实体解码、空白规范化以及文件名安全化等工具函数，
/// 供 TXT / EPUB 导出器共同使用。

#pragma once

#include <string>

namespace novel::text_sanitizer {

/// 将文件名中的非法字符（Windows 保留字符）替换为下划线
std::string sanitize_filename(std::string name);

/// 将包含 HTML 标签和实体的富文本转换为纯文本
///
/// 处理流程：HTML 实体解码 -> 标签剥离（块级元素插入换行） -> 空白行压缩
std::string html_to_plain_text(const std::string& input);

} // namespace novel::text_sanitizer
