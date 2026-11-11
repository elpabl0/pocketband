#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make Pocket Band's installer: one HTML file with the firmware package inside it.

  tools/make_installer.py PACKAGE.fwsc VERSION OUT.html

web/installer.html with web/fm1pkg.js and web/fm1ota.js inlined and the package as a data: URI (the page
checks its SHA-256 before it offers to install). The file opens from the disk in Chrome or Edge: no server,
no internet. tools/build.py makes build/pocket-band-installer.html with it.
"""
import base64
import hashlib
import json
import re
import sys
from pathlib import Path

WEB = Path(__file__).resolve().parents[1] / "web"
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F      # as web/fm1pkg.js: the identity is one marker byte after each of the first 20 blocks


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def make(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    lib = "\n".join(strip_module((WEB / f).read_text(encoding="utf-8")) for f in ("fm1pkg.js", "fm1ota.js"))
    product = "".join(chr((b - i - 1) & 0xFF) for i in range(BLOCKS) if (b := raw[i * BLK + KEEP]) != 0x7D)
    if not product or not re.fullmatch(r"FM-1_\d+", product):
        raise SystemExit(f"{pkg}: not an FM-1 package (identity {product!r})")
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit(f"{pkg}: no update loader in it")
    html = (WEB / "installer.html").read_text(encoding="utf-8")
    meta = json.dumps({"version": version, "product": product, "sha256": hashlib.sha256(raw).hexdigest(),
                       "pkg": "data:application/octet-stream;base64," + base64.b64encode(raw).decode()})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"web/installer.html must contain {mark} once")
    out.write_text(html.replace("/*LIB*/", lib).replace("/*META*/", meta), encoding="utf-8")
    return out


if __name__ == "__main__":
    o = make(*sys.argv[1:4])
    print(f"installer: {o} ({o.stat().st_size} B)")
