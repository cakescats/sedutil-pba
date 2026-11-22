#!/usr/bin/env python3
"""Inline ui/ and dev/mock.js into one dev/preview.html, a page that runs
anywhere a single file opens (no Tauri, no relative paths)."""
import base64, pathlib
here = pathlib.Path(__file__).resolve().parent
ui = here.parent / "ui"
html = (ui / "index.html").read_text()
logo = base64.b64encode((ui / "logo.png").read_bytes()).decode()
html = html.replace('<link rel="stylesheet" href="style.css">', "<style>\n" + (ui / "style.css").read_text() + "</style>")
html = html.replace('src="logo.png"', 'src="data:image/png;base64,' + logo + '"')
html = html.replace('<script src="app.js"></script>',
    "<script>\n" + (here / "mock.js").read_text() + "</script>\n<script>\n" + (ui / "app.js").read_text() + "</script>")
(here / "preview.html").write_text(html)
print(here / "preview.html")
