from __future__ import annotations

import argparse
import sys
from pathlib import Path

from rain_client import DEFAULT_BASE_URL, RainClient, get_apikey


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Login to Rain API V3 web and persist the authenticated cookies."
    )
    parser.add_argument(
        "--base-url",
        default=DEFAULT_BASE_URL,
        help=f"Server base URL, default: {DEFAULT_BASE_URL}",
    )
    parser.add_argument(
        "--env-file",
        default=Path(__file__).with_name(".env"),
        type=Path,
        help="Path to the .env file containing APIKEY",
    )
    parser.add_argument(
        "--cookies-out",
        default=Path(__file__).with_name("rain.session.json"),
        type=Path,
        help="Where to save the session cookies as JSON",
    )
    parser.add_argument(
        "--timeout",
        default=15.0,
        type=float,
        help="HTTP timeout in seconds",
    )
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    try:
        apikey = get_apikey(args.env_file)
        client = RainClient(base_url=args.base_url, timeout=args.timeout)
        user_info = client.login(apikey)
        client.save_cookies(args.cookies_out)
    except Exception as exc:
        print(f"[error] {exc}", file=sys.stderr)
        return 1

    cookie_names = ", ".join(sorted(client.session.cookies.get_dict().keys()))
    remaining = user_info.get("remaining_calls", "<unknown>")

    print("[ok] login succeeded")
    print("[info] redirect: index.php")
    print(f"[info] cookies saved to: {args.cookies_out}")
    print(f"[info] cookie names: {cookie_names}")
    print(f"[info] remaining_calls: {remaining}")
    print("[info] verified markers: ?logout=1, search form")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
