from __future__ import annotations

import argparse
import json
import os
import re
import time
from dataclasses import asdict, dataclass
from html import unescape
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional
from urllib.parse import urljoin

import requests


DEFAULT_BASE_URL = "https://v3.rain.ink"
LOGIN_PATH = "/web/index.php"
BATCH_PATH = "/web/batch.php"
INVALID_KEY_MARKER = "无效的API密钥"

USER_RE = re.compile(r"const user = (\{.*?\});", re.S)
BOOK_RE = re.compile(r"const book = (null|\{.*?\});", re.S)
RESULT_CARD_RE = re.compile(
    r'<img\s+src="(?P<cover_url>[^"]+)"[^>]*alt="cover">.*?'
    r'<h3[^>]*>(?P<title>.*?)</h3>.*?'
    r'<p[^>]*>作者：(?P<author>.*?)</p>.*?'
    r'<p[^>]*>字数：(?P<word_number>.*?)</p>.*?'
    r'<p[^>]*>最新：(?P<latest_chapter>.*?)</p>.*?'
    r'<p[^>]*>章节：(?P<serial_count>\d+)\s*章</p>.*?'
    r'<a\s+href="\?book_id=(?P<book_id>\d+)"',
    re.S,
)


class RainApiError(RuntimeError):
    pass


@dataclass(slots=True)
class SearchBook:
    book_id: str
    title: str
    author: str
    word_number: str
    latest_chapter: str
    serial_count: int
    cover_url: str


@dataclass(slots=True)
class BookDetail:
    book_id: str
    title: str
    author: str
    abstract: str
    category: str
    complete_category: str
    word_number: str
    latest_chapter: str
    serial_count: int
    cover_url: str


@dataclass(slots=True)
class Chapter:
    index: int
    title: str
    content: str


def load_env_file(path: Path) -> Dict[str, str]:
    values: Dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip().strip("'").strip('"')
    return values


def get_apikey(env_path: Path) -> str:
    apikey = os.environ.get("APIKEY")
    if apikey:
        return apikey

    if not env_path.exists():
        raise RainApiError(f"missing API key file: {env_path}")

    apikey = load_env_file(env_path).get("APIKEY", "")
    if not apikey:
        raise RainApiError(f"APIKEY not found in {env_path}")
    return apikey


def _clean_text(value: str) -> str:
    without_tags = re.sub(r"<[^>]+>", "", value)
    return unescape(without_tags).strip()


class RainClient:
    def __init__(
        self,
        base_url: str = DEFAULT_BASE_URL,
        timeout: float = 15.0,
        session: Optional[requests.Session] = None,
        retry_attempts: int = 3,
        retry_delay: float = 0.5,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout
        self.retry_attempts = max(1, retry_attempts)
        self.retry_delay = max(0.0, retry_delay)
        self.session = session or requests.Session()
        self.login_url = urljoin(self.base_url + "/", LOGIN_PATH.lstrip("/"))
        self.batch_url = urljoin(self.base_url + "/", BATCH_PATH.lstrip("/"))
        self.apikey: Optional[str] = None
        self.user_info: Dict[str, Any] = {}

    @classmethod
    def from_env_file(
        cls,
        env_path: Path,
        *,
        base_url: str = DEFAULT_BASE_URL,
        timeout: float = 15.0,
    ) -> "RainClient":
        client = cls(base_url=base_url, timeout=timeout)
        client.login(get_apikey(env_path))
        return client

    def login(self, apikey: str) -> Dict[str, Any]:
        response = self._request(
            "post",
            self.login_url,
            data={"apikey": apikey},
            allow_redirects=False,
        )
        location = response.headers.get("Location", "")

        if response.status_code == 302 and location.endswith("index.php"):
            html = self._request("get", self.login_url).text
            self._assert_authenticated_html(html)
            self.apikey = apikey
            self.user_info = self._extract_user_info(html)
            return self.user_info

        if INVALID_KEY_MARKER in response.text:
            raise RainApiError("login failed: invalid API key")

        raise RainApiError(
            "login failed: unexpected response "
            f"(status={response.status_code}, location={location!r})"
        )

    def save_cookies(self, path: Path) -> None:
        path.write_text(
            json.dumps(self.session.cookies.get_dict(), indent=2, ensure_ascii=False),
            encoding="utf-8",
        )

    def check_remaining(self) -> int:
        self._require_login()
        data = self._post_json(
            self.login_url,
            {"checkRemaining": 1, "apikey": self.apikey},
        )
        return int(data["remaining_calls"])

    def refresh_remaining(self) -> int:
        self._require_login()
        data = self._post_json(self.login_url, {"refreshRemaining": 1})
        return int(data["remaining_calls"])

    def search_books(self, keyword: str) -> List[SearchBook]:
        self._require_login()
        response = self._request("post", self.login_url, data={"keyword": keyword})
        html = response.text
        self._assert_authenticated_html(html)
        self.user_info = self._extract_user_info(html) or self.user_info

        results: List[SearchBook] = []
        for match in RESULT_CARD_RE.finditer(html):
            results.append(
                SearchBook(
                    book_id=match.group("book_id"),
                    title=_clean_text(match.group("title")),
                    author=_clean_text(match.group("author")),
                    word_number=_clean_text(match.group("word_number")),
                    latest_chapter=_clean_text(match.group("latest_chapter")),
                    serial_count=int(match.group("serial_count")),
                    cover_url=match.group("cover_url").strip(),
                )
            )
        return results

    def get_book_detail(
        self,
        book_id: str,
        *,
        fallback: Optional[SearchBook] = None,
    ) -> BookDetail:
        self._require_login()
        response = self._request("get", self.login_url, params={"book_id": str(book_id)})
        html = response.text
        self._assert_authenticated_html(html)

        match = BOOK_RE.search(html)
        if not match:
            if fallback is not None:
                return self._book_detail_from_search_book(fallback)
            raise RainApiError("book detail not found in detail page HTML")

        raw_value = match.group(1)
        if raw_value == "null":
            if fallback is not None:
                return self._book_detail_from_search_book(fallback)
            raise RainApiError(f"book detail unavailable for book_id={book_id}")

        try:
            payload = json.loads(raw_value)
        except json.JSONDecodeError as exc:
            if fallback is not None:
                return self._book_detail_from_search_book(fallback)
            raise RainApiError("failed to parse book detail JSON") from exc

        return self._book_detail_from_payload(payload, fallback=fallback)

    def get_batch_count(self, book_id: str) -> int:
        self._require_login()
        data = self._post_json(self.batch_url, {"id": str(book_id)})
        return int(data.get("batch", 0))

    def get_batch(self, book_id: str, batch: int) -> List[Dict[str, str]]:
        self._require_login()
        data = self._post_json(
            self.batch_url,
            {"id": str(book_id), "batch": int(batch)},
        )
        if "error" in data:
            raise RainApiError(f"batch download failed: {data['error']}")

        items = data.get("data")
        if not isinstance(items, list):
            raise RainApiError(f"unexpected batch payload: {data!r}")

        result: List[Dict[str, str]] = []
        for item in items:
            result.append(
                {
                    "title": _clean_text(str(item.get("title", ""))),
                    "content": str(item.get("content", "")),
                }
            )
        return result

    def download_book(
        self,
        book_id: str,
        *,
        on_batch: Optional[Callable[[int, int, int, int], None]] = None,
    ) -> List[Chapter]:
        total_batches = self.get_batch_count(book_id)
        if total_batches <= 0:
            return []

        chapters: List[Chapter] = []
        for batch_no in range(1, total_batches + 1):
            batch_items = self.get_batch(book_id, batch_no)
            for item in batch_items:
                chapters.append(
                    Chapter(
                        index=len(chapters) + 1,
                        title=item["title"],
                        content=item["content"],
                    )
                )
            if on_batch is not None:
                on_batch(batch_no, total_batches, len(batch_items), len(chapters))
        return chapters

    def download_binary(self, url: str) -> tuple[bytes, str]:
        response = self._request("get", url)
        media_type = response.headers.get("Content-Type", "application/octet-stream")
        media_type = media_type.split(";", 1)[0].strip().lower() or "application/octet-stream"
        return response.content, media_type

    def _require_login(self) -> None:
        if not self.apikey:
            raise RainApiError("not logged in")

    def _assert_authenticated_html(self, html: str) -> None:
        if INVALID_KEY_MARKER in html:
            raise RainApiError("session rejected by server")
        has_logged_in_marker = "?logout=1" in html and (
            'const user =' in html or 'name="keyword"' in html or 'downloadBtn' in html
        )
        if not has_logged_in_marker:
            raise RainApiError("unexpected HTML: authenticated markers not found")

    def _extract_user_info(self, html: str) -> Dict[str, Any]:
        match = USER_RE.search(html)
        if not match:
            return {}
        try:
            return json.loads(match.group(1))
        except json.JSONDecodeError:
            return {}

    def _book_detail_from_search_book(self, book: SearchBook) -> BookDetail:
        return BookDetail(
            book_id=book.book_id,
            title=book.title,
            author=book.author,
            abstract="",
            category="",
            complete_category="",
            word_number=book.word_number,
            latest_chapter=book.latest_chapter,
            serial_count=book.serial_count,
            cover_url=book.cover_url,
        )

    def _book_detail_from_payload(
        self,
        payload: Dict[str, Any],
        *,
        fallback: Optional[SearchBook] = None,
    ) -> BookDetail:
        fallback_detail = (
            self._book_detail_from_search_book(fallback) if fallback is not None else None
        )

        def pick_text(*values: Any) -> str:
            for value in values:
                if value is None:
                    continue
                text = _clean_text(str(value))
                if text:
                    return text
            return ""

        def pick_int(*values: Any) -> int:
            for value in values:
                if value in (None, ""):
                    continue
                try:
                    return int(value)
                except (TypeError, ValueError):
                    continue
            return 0

        return BookDetail(
            book_id=str(payload.get("book_id") or (fallback_detail.book_id if fallback_detail else "")),
            title=pick_text(payload.get("book_name"), fallback_detail.title if fallback_detail else ""),
            author=pick_text(payload.get("author"), fallback_detail.author if fallback_detail else ""),
            abstract=pick_text(payload.get("abstract"), payload.get("book_abstract_v2")),
            category=pick_text(payload.get("category")),
            complete_category=pick_text(payload.get("complete_category")),
            word_number=pick_text(
                payload.get("word_number"),
                fallback_detail.word_number if fallback_detail else "",
            ),
            latest_chapter=pick_text(
                payload.get("last_chapter_title"),
                payload.get("last_chapter_item"),
                fallback_detail.latest_chapter if fallback_detail else "",
            ),
            serial_count=pick_int(
                payload.get("serial_count"),
                payload.get("chapter_number"),
                fallback_detail.serial_count if fallback_detail else 0,
            ),
            cover_url=pick_text(
                payload.get("thumb_url"),
                payload.get("thumb_url_hd"),
                payload.get("audio_thumb_url_hd"),
                fallback_detail.cover_url if fallback_detail else "",
            ),
        )

    def _post_json(self, url: str, data: Dict[str, Any]) -> Dict[str, Any]:
        response = self._request("post", url, data=data)
        try:
            payload = response.json()
        except ValueError as exc:
            snippet = response.text[:300].replace("\n", " ")
            raise RainApiError(
                f"expected JSON from {url}, got status {response.status_code}: {snippet}"
            ) from exc

        if not isinstance(payload, dict):
            raise RainApiError(f"unexpected JSON payload from {url}: {payload!r}")
        return payload

    def _request(self, method: str, url: str, **kwargs: Any) -> requests.Response:
        kwargs.setdefault("timeout", self.timeout)
        last_error: Optional[Exception] = None

        for attempt in range(1, self.retry_attempts + 1):
            try:
                return self.session.request(method, url, **kwargs)
            except requests.RequestException as exc:
                last_error = exc
                if attempt >= self.retry_attempts:
                    raise
                time.sleep(self.retry_delay * attempt)

        raise RainApiError(f"request failed without response: {url}") from last_error


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Rain API V3 Python client")
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

    subparsers = parser.add_subparsers(dest="command", required=True)

    search_parser = subparsers.add_parser("search", help="Search books by keyword")
    search_parser.add_argument("keyword", help="Keyword to search")

    download_parser = subparsers.add_parser(
        "download",
        help="Download all chapters for a book id as JSON",
    )
    download_parser.add_argument("book_id", help="Target book_id")
    download_parser.add_argument(
        "--out",
        type=Path,
        help="Optional JSON output path; prints to stdout when omitted",
    )

    subparsers.add_parser("remaining", help="Show remaining quota")
    return parser


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()

    try:
        client = RainClient.from_env_file(
            args.env_file,
            base_url=args.base_url,
            timeout=args.timeout,
        )

        if args.command == "search":
            books = client.search_books(args.keyword)
            print(
                json.dumps(
                    [asdict(book) for book in books],
                    ensure_ascii=False,
                    indent=2,
                )
            )
            return 0

        if args.command == "download":
            chapters = client.download_book(
                args.book_id,
                on_batch=lambda current, total, batch_size, chapter_total: print(
                    f"[info] batch {current}/{total}: +{batch_size} chapters "
                    f"(total {chapter_total})"
                ),
            )
            payload = [asdict(chapter) for chapter in chapters]
            if args.out:
                args.out.write_text(
                    json.dumps(payload, ensure_ascii=False, indent=2),
                    encoding="utf-8",
                )
                print(f"[ok] wrote {len(chapters)} chapters to {args.out}")
            else:
                print(json.dumps(payload, ensure_ascii=False, indent=2))
            return 0

        if args.command == "remaining":
            print(json.dumps({"remaining_calls": client.check_remaining()}, ensure_ascii=False))
            return 0
    except Exception as exc:
        print(f"[error] {exc}")
        return 1

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
