#!/usr/bin/env python3
"""
Download HD photos from picsum.photos API into ./test_image.

Usage:
    python3 dowload_test_photos.py --count 5 --width 1920 --height 1080

The script avoids extra dependencies by using urllib from the standard library.
Picsum API: https://picsum.photos
"""

from __future__ import annotations

import argparse
import os
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path
from typing import Optional


def build_url(width: int, height: int) -> str:
    # picsum.photos random endpoint; add timestamp to reduce caching
    ts = int(time.time() * 1000)
    return f"https://picsum.photos/{width}/{height}?random={ts}"


def download_one(url: str, dest: Path, idx: int, total: int) -> bool:
    print(f"[info] ({idx}/{total}) fetching {url}")
    headers = {
        "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
        "Accept": "image/avif,image/webp,image/apng,image/svg+xml,image/*,*/*;q=0.8",
        "Referer": "https://picsum.photos/",
        "Accept-Language": "en-US,en;q=0.9",
    }
    req = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            if resp.status != 200:
                print(f"[warn] skip {url} (HTTP {resp.status} {resp.reason})", file=sys.stderr)
                return False
            data = resp.read()
    except urllib.error.URLError as e:
        print(f"[warn] fetch failed {url}: {e}", file=sys.stderr)
        return False

    dest.write_bytes(data)
    print(f"[ok] ({idx}/{total}) saved {dest}")
    return True


def main():
    parser = argparse.ArgumentParser(description="Download HD photos from picsum.photos API.")
    parser.add_argument("--count", type=int, default=5, help="number of images to download")
    parser.add_argument("--width", type=int, default=1920, help="image width")
    parser.add_argument("--height", type=int, default=1080, help="image height")
    parser.add_argument("--out", type=Path, default=Path("test_image"), help="output folder")
    args = parser.parse_args()

    args.out.mkdir(parents=True, exist_ok=True)

    total = max(1, args.count)
    for i in range(total):
        url = build_url(args.width, args.height)
        filename = f"picsum_{int(time.time())}_{i+1}.jpg"
        dest = args.out / filename
        download_one(url, dest, i + 1, total)


if __name__ == "__main__":
    main()
