#!/usr/bin/env python3
"""Capture a screenshot of the prismarine-viewer running on localhost:3007."""

import sys
from playwright.sync_api import sync_playwright

VIEWER_URL = "http://localhost:3007"
DEFAULT_OUT = "/tmp/prometheus_vibe.png"


def capture(out_path: str) -> bool:
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page(viewport={"width": 1280, "height": 720})
        page.goto(VIEWER_URL, wait_until="networkidle")
        # Give the 3D scene a moment to render
        page.wait_for_timeout(2000)
        page.screenshot(path=out_path)
        browser.close()
    return True


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUT
    try:
        if capture(out):
            print(f"Screenshot saved to {out}")
            sys.exit(0)
    except Exception as e:
        print(f"Error capturing view: {e}", file=sys.stderr)
    sys.exit(1)
