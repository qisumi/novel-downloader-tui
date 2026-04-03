from __future__ import annotations

import re
import zipfile
from datetime import datetime, timezone
from html import unescape
from pathlib import Path
from typing import Iterable, Optional
from uuid import uuid4

from rain_client import BookDetail, Chapter


INVALID_FILENAME_CHARS = '<>:"/\\|?*'
BLOCK_TAG_RE = re.compile(
    r"(?is)<\s*/?\s*(?:p|div|br|li|section|article|footer|header|h1|h2|h3)\b[^>]*>"
)
TAG_RE = re.compile(r"(?is)<[^>]+>")


def sanitize_filename(name: str) -> str:
    cleaned = "".join("_" if ch in INVALID_FILENAME_CHARS else ch for ch in name)
    cleaned = cleaned.strip().rstrip(".")
    return cleaned or "book"


def html_to_plain_text(text: str) -> str:
    text = BLOCK_TAG_RE.sub("\n", text)
    text = TAG_RE.sub("", text)
    text = unescape(text)
    text = text.replace("\r\n", "\n").replace("\r", "\n")

    lines = []
    for raw_line in text.split("\n"):
        line = re.sub(r"[ \t\f\v]+", " ", raw_line).strip()
        if line:
            lines.append(line)
    return "\n".join(lines)


def normalize_inline_text(text: str) -> str:
    return re.sub(r"\s+", " ", html_to_plain_text(text)).strip()


def chapter_to_view(chapter: Chapter) -> tuple[str, list[str]]:
    title = normalize_inline_text(chapter.title) or f"第 {chapter.index} 章"
    content_text = html_to_plain_text(chapter.content)
    paragraphs = [line for line in content_text.split("\n") if line.strip()]

    if paragraphs and normalize_inline_text(paragraphs[0]) == title:
        paragraphs = paragraphs[1:]

    if not paragraphs:
        paragraphs = [""]

    return title, paragraphs


def category_text(book: BookDetail) -> str:
    parts = [part for part in [book.category, book.complete_category] if part]
    return " / ".join(parts)


def export_txt(
    book: BookDetail,
    chapters: Iterable[Chapter],
    output_dir: Path,
    *,
    filename_suffix: str = "",
) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    out_path = output_dir / f"{sanitize_filename(book.title)}{filename_suffix}.txt"

    chapter_list = list(chapters)
    with out_path.open("w", encoding="utf-8-sig", newline="\n") as fh:
        fh.write(f"{book.title}\n")
        if book.author:
            fh.write(f"作者：{book.author}\n")
        category = category_text(book)
        if category:
            fh.write(f"分类：{category}\n")
        if book.word_number:
            fh.write(f"字数：{book.word_number}\n")
        if book.serial_count:
            fh.write(f"章节：{book.serial_count} 章\n")
        fh.write("\n")

        if book.abstract:
            fh.write("简介：\n")
            fh.write(f"{html_to_plain_text(book.abstract)}\n\n")

        fh.write("========================================\n\n")

        for chapter in chapter_list:
            title, paragraphs = chapter_to_view(chapter)
            fh.write(f"{title}\n\n")
            fh.write("\n\n".join(paragraphs).strip())
            fh.write("\n\n")

    return out_path.resolve()


def xml_escape(text: str) -> str:
    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
        .replace("'", "&apos;")
    )


def cover_extension(media_type: str) -> str:
    mapping = {
        "image/jpeg": "jpg",
        "image/jpg": "jpg",
        "image/png": "png",
        "image/webp": "webp",
        "image/gif": "gif",
    }
    return mapping.get(media_type.lower(), "bin")


def make_epub_stylesheet() -> str:
    return """body {
  font-family: "Noto Serif CJK SC", "Source Han Serif SC", serif;
  font-size: 1em;
  line-height: 1.8;
  margin: 1em 2em;
  color: #222;
  background: #fafaf8;
}
h1, h2 { color: #333; }
p { text-indent: 2em; margin: 0.4em 0; }
.cover-page { text-align: center; padding-top: 12%; }
.cover-image { max-width: 78%; max-height: 70vh; display: block; margin: 0 auto 2em; }
.book-title  { font-size: 2em; margin-bottom: 0.4em; }
.book-author { font-size: 1.2em; color: #666; margin-bottom: 1em; }
.book-meta { font-size: 0.95em; color: #666; }
.book-abstract { font-size: 0.95em; color: #555; margin: 2em auto 0; max-width: 36em; text-align: left; }
.chapter-title { margin: 1em 0; border-bottom: 1px solid #ccc; padding-bottom: 0.3em; }
"""


def export_epub(
    book: BookDetail,
    chapters: Iterable[Chapter],
    output_dir: Path,
    *,
    filename_suffix: str = "",
    cover_bytes: Optional[bytes] = None,
    cover_media_type: Optional[str] = None,
) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    out_path = output_dir / f"{sanitize_filename(book.title)}{filename_suffix}.epub"

    chapter_list = list(chapters)
    chapter_views = [chapter_to_view(chapter) for chapter in chapter_list]
    modified = datetime.now(timezone.utc)
    modified_iso = modified.strftime("%Y-%m-%dT%H:%M:%SZ")
    date_iso = modified.strftime("%Y-%m-%d")
    book_uuid = f"rain-{book.book_id or uuid4()}"

    cover_item = ""
    cover_meta = ""
    cover_href = ""
    if cover_bytes and cover_media_type:
        ext = cover_extension(cover_media_type)
        cover_href = f"images/cover.{ext}"
        cover_item = (
            f'    <item id="cover-image" href="{cover_href}" '
            f'media-type="{xml_escape(cover_media_type)}" properties="cover-image"/>\n'
        )
        cover_meta = '    <meta name="cover" content="cover-image"/>\n'

    category = category_text(book)
    manifest_items = []
    spine_items = ['    <itemref idref="cover-page"/>', '    <itemref idref="nav"/>']
    nav_items = ['<li><a href="cover.xhtml">封面</a></li>']
    ncx_points = [
        '    <navPoint id="nav-cover" playOrder="1">\n'
        '      <navLabel><text>封面</text></navLabel>\n'
        '      <content src="cover.xhtml"/>\n'
        '    </navPoint>\n'
    ]

    for index, (title, _) in enumerate(chapter_views, start=1):
        manifest_items.append(
            f'    <item id="ch{index}" href="chapter_{index}.xhtml" '
            'media-type="application/xhtml+xml"/>'
        )
        spine_items.append(f'    <itemref idref="ch{index}"/>')
        nav_items.append(
            f'<li><a href="chapter_{index}.xhtml">{xml_escape(title)}</a></li>'
        )
        ncx_points.append(
            f'    <navPoint id="nav-{index}" playOrder="{index + 1}">\n'
            f'      <navLabel><text>{xml_escape(title)}</text></navLabel>\n'
            f'      <content src="chapter_{index}.xhtml"/>\n'
            '    </navPoint>\n'
        )

    opf = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<package version="3.0" xmlns="http://www.idpf.org/2007/opf" '
        'unique-identifier="book-id" xml:lang="zh-CN">\n'
        '  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">\n'
        f'    <dc:identifier id="book-id">{xml_escape(book_uuid)}</dc:identifier>\n'
        f'    <dc:title>{xml_escape(book.title)}</dc:title>\n'
        f'    <dc:creator>{xml_escape(book.author or "未知作者")}</dc:creator>\n'
        '    <dc:language>zh-CN</dc:language>\n'
        f'    <dc:date>{date_iso}</dc:date>\n'
        f'    <dc:description>{xml_escape(html_to_plain_text(book.abstract))}</dc:description>\n'
        + (f'    <dc:subject>{xml_escape(category)}</dc:subject>\n' if category else "")
        + cover_meta
        + f'    <meta property="dcterms:modified">{modified_iso}</meta>\n'
        '  </metadata>\n'
        '  <manifest>\n'
        '    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>\n'
        '    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>\n'
        '    <item id="css" href="style.css" media-type="text/css"/>\n'
        '    <item id="cover-page" href="cover.xhtml" media-type="application/xhtml+xml"/>\n'
        + cover_item
        + ("\n".join(manifest_items) + "\n" if manifest_items else "")
        + '  </manifest>\n'
        '  <spine toc="ncx">\n'
        + "\n".join(spine_items)
        + '\n  </spine>\n'
        '</package>\n'
    )

    nav_xhtml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<!DOCTYPE html>\n'
        '<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops" lang="zh-CN">\n'
        '<head><meta charset="UTF-8"/><title>目录</title><link rel="stylesheet" href="style.css"/></head>\n'
        '<body>\n'
        '<nav epub:type="toc" id="toc"><h1>目录</h1><ol>\n'
        + "\n".join(nav_items)
        + '\n</ol></nav>\n'
        '</body>\n'
        '</html>\n'
    )

    ncx = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<!DOCTYPE ncx PUBLIC "-//NISO//DTD ncx 2005-1//EN" '
        '"http://www.daisy.org/z3986/2005/ncx-2005-1.dtd">\n'
        '<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">\n'
        '  <head>\n'
        f'    <meta name="dtb:uid" content="{xml_escape(book_uuid)}"/>\n'
        '    <meta name="dtb:depth" content="1"/>\n'
        '    <meta name="dtb:totalPageCount" content="0"/>\n'
        '    <meta name="dtb:maxPageNumber" content="0"/>\n'
        '  </head>\n'
        f'  <docTitle><text>{xml_escape(book.title)}</text></docTitle>\n'
        '  <navMap>\n'
        + "".join(ncx_points)
        + '  </navMap>\n'
        '</ncx>\n'
    )

    cover_parts = []
    if cover_href:
        cover_parts.append(
            f'<img class="cover-image" src="{xml_escape(cover_href)}" alt="{xml_escape(book.title)}"/>'
        )
    cover_parts.append(f'<h1 class="book-title">{xml_escape(book.title)}</h1>')
    if book.author:
        cover_parts.append(f'<p class="book-author">{xml_escape(book.author)}</p>')
    meta_parts = []
    if category:
        meta_parts.append(f"分类：{xml_escape(category)}")
    if book.word_number:
        meta_parts.append(f"字数：{xml_escape(book.word_number)}")
    if book.serial_count:
        meta_parts.append(f"章节：{book.serial_count} 章")
    if meta_parts:
        cover_parts.append(f'<p class="book-meta">{" | ".join(meta_parts)}</p>')
    if book.abstract:
        cover_parts.append(
            f'<p class="book-abstract">{xml_escape(html_to_plain_text(book.abstract))}</p>'
        )

    cover_xhtml = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<!DOCTYPE html>\n'
        '<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN">\n'
        '<head><meta charset="UTF-8"/><title>封面</title><link rel="stylesheet" href="style.css"/></head>\n'
        '<body class="cover-page">\n'
        + "\n".join(cover_parts)
        + '\n</body>\n'
        '</html>\n'
    )

    with zipfile.ZipFile(out_path, "w") as zf:
        mimetype_info = zipfile.ZipInfo("mimetype")
        mimetype_info.compress_type = zipfile.ZIP_STORED
        zf.writestr(mimetype_info, "application/epub+zip")

        zf.writestr(
            "META-INF/container.xml",
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">\n'
            '  <rootfiles>\n'
            '    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>\n'
            '  </rootfiles>\n'
            '</container>\n',
            compress_type=zipfile.ZIP_DEFLATED,
        )
        zf.writestr("OEBPS/content.opf", opf, compress_type=zipfile.ZIP_DEFLATED)
        zf.writestr("OEBPS/nav.xhtml", nav_xhtml, compress_type=zipfile.ZIP_DEFLATED)
        zf.writestr("OEBPS/toc.ncx", ncx, compress_type=zipfile.ZIP_DEFLATED)
        zf.writestr("OEBPS/cover.xhtml", cover_xhtml, compress_type=zipfile.ZIP_DEFLATED)
        zf.writestr("OEBPS/style.css", make_epub_stylesheet(), compress_type=zipfile.ZIP_DEFLATED)

        if cover_href and cover_bytes:
            zf.writestr(f"OEBPS/{cover_href}", cover_bytes, compress_type=zipfile.ZIP_DEFLATED)

        for index, (title, paragraphs) in enumerate(chapter_views, start=1):
            chapter_xhtml = (
                '<?xml version="1.0" encoding="UTF-8"?>\n'
                '<!DOCTYPE html>\n'
                '<html xmlns="http://www.w3.org/1999/xhtml" lang="zh-CN">\n'
                '<head><meta charset="UTF-8"/>'
                f'<title>{xml_escape(title)}</title>'
                '<link rel="stylesheet" href="style.css"/></head>\n'
                '<body>\n'
                f'<h2 class="chapter-title">{xml_escape(title)}</h2>\n'
                + "\n".join(f"<p>{xml_escape(paragraph)}</p>" for paragraph in paragraphs if paragraph)
                + '\n</body>\n'
                '</html>\n'
            )
            zf.writestr(
                f"OEBPS/chapter_{index}.xhtml",
                chapter_xhtml,
                compress_type=zipfile.ZIP_DEFLATED,
            )

    return out_path.resolve()
