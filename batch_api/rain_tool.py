from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Sequence

from rain_client import DEFAULT_BASE_URL, BookDetail, RainApiError, RainClient, SearchBook
from rain_export import export_epub, export_txt


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Search, choose, download, and export books from Rain API V3."
    )
    parser.add_argument("keyword", help="Keyword to search")
    parser.add_argument(
        "--env-file",
        default=Path(__file__).with_name(".env"),
        type=Path,
        help="Path to the .env file containing APIKEY",
    )
    parser.add_argument(
        "--base-url",
        default=DEFAULT_BASE_URL,
        help=f"Server base URL, default: {DEFAULT_BASE_URL}",
    )
    parser.add_argument(
        "--timeout",
        default=15.0,
        type=float,
        help="HTTP timeout in seconds",
    )
    parser.add_argument(
        "--pick",
        type=int,
        help="1-based index from the search result list",
    )
    parser.add_argument(
        "--book-id",
        help="Pick the matching book_id from the search results",
    )
    parser.add_argument(
        "--format",
        choices=["txt", "epub", "both"],
        default="both",
        help="Export format, default: both",
    )
    parser.add_argument(
        "--output-dir",
        default=Path(__file__).with_name("output"),
        type=Path,
        help="Output directory for exported files",
    )
    parser.add_argument(
        "--filename-suffix",
        default="",
        help="Optional suffix appended before the output file extension",
    )
    parser.add_argument(
        "--non-interactive",
        action="store_true",
        help="Do not prompt; require --pick or --book-id when multiple results exist",
    )
    return parser


def print_search_results(books: Sequence[SearchBook]) -> None:
    print("[info] search results:")
    for index, book in enumerate(books, start=1):
        print(
            f"  [{index}] {book.title} | {book.author} | {book.serial_count} 章 | "
            f"{book.word_number} | id={book.book_id}"
        )


def choose_book(
    books: Sequence[SearchBook],
    *,
    pick: int | None,
    book_id: str | None,
    non_interactive: bool,
) -> SearchBook:
    if not books:
        raise RainApiError("no books found for the given keyword")

    if book_id:
        for book in books:
            if book.book_id == book_id:
                return book
        raise RainApiError(f"book_id not found in search results: {book_id}")

    if pick is not None:
        if pick < 1 or pick > len(books):
            raise RainApiError(f"--pick out of range: {pick}")
        return books[pick - 1]

    if len(books) == 1:
        print("[info] only one search result, auto-selected #1")
        return books[0]

    print_search_results(books)
    if non_interactive or not sys.stdin.isatty():
        raise RainApiError("multiple results found; pass --pick or --book-id")

    while True:
        raw = input(f"Select a book [1-{len(books)}], default 1: ").strip()
        if not raw:
            return books[0]
        if raw.isdigit():
            value = int(raw)
            if 1 <= value <= len(books):
                return books[value - 1]
        print("[warn] invalid selection")


def print_selected(book: SearchBook, detail: BookDetail) -> None:
    print(f"[info] selected: {book.title} ({book.book_id})")
    print(f"[info] author: {detail.author or book.author}")
    if detail.complete_category or detail.category:
        category = " / ".join(x for x in [detail.category, detail.complete_category] if x)
        print(f"[info] category: {category}")
    if detail.word_number or book.word_number:
        print(f"[info] word count: {detail.word_number or book.word_number}")
    print(f"[info] chapters: {detail.serial_count or book.serial_count}")


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        client = RainClient.from_env_file(
            args.env_file,
            base_url=args.base_url,
            timeout=args.timeout,
        )

        books = client.search_books(args.keyword)
        selected = choose_book(
            books,
            pick=args.pick,
            book_id=args.book_id,
            non_interactive=args.non_interactive,
        )
        detail = client.get_book_detail(selected.book_id, fallback=selected)
        print_selected(selected, detail)

        remaining = client.check_remaining()
        needed = detail.serial_count or selected.serial_count
        print(f"[info] remaining calls: {remaining}")
        if needed and remaining < needed:
            raise RainApiError(
                f"remaining calls not enough for this book: need {needed}, have {remaining}"
            )

        chapters = client.download_book(
            selected.book_id,
            on_batch=lambda current, total, batch_size, chapter_total: print(
                f"[info] batch {current}/{total}: +{batch_size} chapters (total {chapter_total})"
            ),
        )
        if not chapters:
            raise RainApiError("no chapters returned from batch API")
        print(f"[info] downloaded chapters: {len(chapters)}")

        output_paths = []
        if args.format in {"txt", "both"}:
            txt_path = export_txt(
                detail,
                chapters,
                args.output_dir,
                filename_suffix=args.filename_suffix,
            )
            output_paths.append(txt_path)

        if args.format in {"epub", "both"}:
            cover_bytes = None
            cover_media_type = None
            if detail.cover_url:
                try:
                    cover_bytes, cover_media_type = client.download_binary(detail.cover_url)
                except Exception as exc:
                    print(f"[warn] cover download failed, exporting EPUB without cover: {exc}")

            epub_path = export_epub(
                detail,
                chapters,
                args.output_dir,
                filename_suffix=args.filename_suffix,
                cover_bytes=cover_bytes,
                cover_media_type=cover_media_type,
            )
            output_paths.append(epub_path)

        remaining_after = client.refresh_remaining()
        print(f"[info] remaining calls after download: {remaining_after}")
        for path in output_paths:
            print(f"[ok] exported: {path}")
        return 0
    except Exception as exc:
        print(f"[error] {exc}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
