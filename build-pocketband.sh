#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Build SLOOP with Pocket Band (firmware/src/pocketband.c, pb_brain.c).
# The app slot is nearly full, so this build gives up two cosmetic things to make room for the generator:
#   FELUCCA_LOGO_TEXT=1  the boot logo drawn as text instead of a picture (4.6 KB)
#   FELUCCA_ICONS=0      no parameter icons on SLOOP's pages; the labels get their full width (4.5 KB)
# Everything else is SLOOP as it is. See BUILDING.md for the toolchain and the SDK.
set -e
cd "$(dirname "$0")"
export FELUCCA_LOGO_TEXT=1 FELUCCA_ICONS=0
exec sh build.sh "$@"
