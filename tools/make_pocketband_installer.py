#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Make a one-file guided installer for Pocket Band.

  make_installer.py SLOOP_TREE PACKAGE.fwsc OUT.html

Runs SLOOP's own web/make_site.py, then folds the firmware package into its installer page as a data: URI
(the page still checks the package's SHA-256), drops what needs a web server (the offline app), points the
editor links at SLOOP's site and adds the steps for this build at the top. The result opens from the disk
in Chrome or Edge: no server, no internet.
"""
import base64
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

GUIDE = """
  <section style="border:2px solid var(--yellow);padding:14px 16px;margin:18px 0;border-radius:6px">
    <h2 style="margin:0 0 8px;color:var(--yellow)">Before you install</h2>
    <p style="margin:0 0 10px">Pocket Band is experimental. This version has been tested on a computer against the real sound
    engines, not yet on a real FM-1. Install it at your own risk.</p>
    <ol style="margin:0 0 10px;padding-left:22px;line-height:1.6">
      <li><b>Back up first.</b> If the FM-1 holds anything you want to keep, save it with the tool of the firmware it runs now.</li>
      <li><b>Connect.</b> A USB-C data cable straight to the computer, no hub. Charge the FM-1 or keep it powered. Close any other app or tab that uses MIDI.</li>
      <li><b>Use Chrome or Edge</b> on a computer, and allow MIDI access when the browser asks.</li>
      <li><b>Press INSTALL below</b> and leave the FM-1 alone until the page says it is done. Do not unplug it.</li>
      <li><b>Try it.</b> Press GLO for the map, walk with KNOB 1 and KNOB 2, press GLO again to travel.</li>
    </ol>
    <p style="margin:0 0 6px"><b>If the FM-1 does not start afterwards:</b> switch it off, hold OCT&minus; alone and switch it on
    (USB rescue), then press INSTALL again.</p>
    <p style="margin:0"><b>To go back:</b> install SLOOP from <a href="https://isod89.github.io/sloop-fm1/" target="_blank" rel="noopener">its own page</a>,
    or the original firmware with M-VAVE's M-UPGRADE.</p>
  </section>
"""

LOGO = '<div class="logo" style="font:700 40px/1 ui-monospace,Menlo,Consolas,monospace;letter-spacing:.08em;padding:18px 0">POCKET <span style="color:var(--red)">BAND</span></div>'
LEAD = ('<p class="lead">A firmware for the M-VAVE FM-1 that writes music with you: pick a place on a map of genre towns and '
        'it writes a four-piece sketch, then re-roll, audition, jam, perform and edit it. Its sound engines and sequencer '
        'are those of SLOOP 2.8.5 by isod89, based on Felucca by Leo Kuroshita (GPL-3.0).</p>')



def one(html, pattern, repl, what, flags=0):
    html, n = re.subn(pattern, repl, html, count=1, flags=flags)
    if n != 1:
        raise SystemExit(f"installer page: {what} not found; update make_installer.py")
    return html


def main():
    tree, pkg, out = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    data = pkg.read_bytes()
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([sys.executable, str(tree / "web" / "make_site.py"), str(pkg), "pocketband", tmp], check=True,
                       stdout=subprocess.DEVNULL)
        html = (Path(tmp) / "webapp" / "installer" / "index.html").read_text(encoding="utf-8")
    sha = hashlib.sha256(data).hexdigest()
    if sha not in html:
        raise SystemExit("installer page: the package's SHA-256 is not in it")
    uri = "data:application/octet-stream;base64," + base64.b64encode(data).decode()
    html = one(html, r'"pkg": "[^"]*"', lambda m: '"pkg": "' + uri + '"', "the package path")
    html = one(html, r"<!--PWA-->.*?<!--/PWA-->", "", "the app manifest", re.S)
    html = one(html, r'if \("serviceWorker" in navigator[^\n]*\n[^\n]*\n', "", "the service worker")
    html = one(html, r'(<div class="strip">.*?</div>)', lambda m: m.group(1) + GUIDE, "the colour strip", re.S)
    html = one(html, r'<div class="logo">.*?</svg>\s*</div>', lambda m: LOGO, "the logo", re.S)
    html = one(html, r'<p class="lead">.*?</p>', lambda m: LEAD, "the lead paragraph", re.S)
    html = one(html, r'<div class="tiles">.*?</div>\s*</div>', "", "the feature tiles", re.S)
    html = one(html, r'<a class="ed" href="\.\./editor/"[^\n]*\n', "", "the editor link")
    html = one(html, r'<p class="small"><a href="\.\./editor/sloop-editor\.html"[^\n]*\n', "", "the offline editor link")
    html = one(html, r"say\(`SLOOP \$\{meta\.version\}`\)", lambda m: 'say("POCKET BAND: READY TO INSTALL")', "the ready message")
    html = one(html, r"<title>[^<]*</title>", "<title>Pocket Band installer</title>", "the title")
    out.write_text(html, encoding="utf-8")
    print(f"installer: {out} ({out.stat().st_size} B), package {len(data)} B, sha256 {sha}")


if __name__ == "__main__":
    main()
