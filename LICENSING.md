# Pocket Band licensing

Pocket Band is free software under the GNU General Public License, version 3 only (`GPL-3.0-only`, full
text in `LICENSE`). It is derived from [SLOOP](https://github.com/isod89/sloop-fm1) 2.8.5 by isod89, which
is derived from [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita, Hügelton Instruments;
both are GPL-3.0-only. Who made what: [CREDITS.md](CREDITS.md).

The rest of this file is Felucca's licensing statement as SLOOP carried it, kept because it still governs
the code Pocket Band inherits. Changes made for Pocket Band: the rows for material that is no longer in
this tree (the icon atlas, the panel image, the web editor and its fonts) are removed, and this
introduction is added. Where it says "Felucca", read the inherited code in this tree.

Pocket Band's own files (`firmware/src/pb_*.c`, `pb_brain.h`, `tests/pb_*.c`, `web/installer.html`'s
changes, `tools/make_installer.py`, `prototype/`, the guides) are Copyright (C) 2026 the Pocket Band
authors, GPL-3.0-only.

## Felucca licensing

Felucca is free software. Its **code** is licensed under the GNU General Public License,
version 3 only (`GPL-3.0-only`, full text in `LICENSE`). Its **assets** are not part of
that licence: the drum sounds made by `tools/gen_waves.py` (the Hügelton Sample Pack) are
Copyright (C) 2026 Hügelton Instruments, all rights reserved. Their licence terms will be published
later. The firmware built from this tree does not contain the Hügelton Sample Pack: its sampled drum kit
is made of CC0 recordings (`assets/samples-cc0/KIT`), and `gen_waves.py` only feeds the SLICE engine's
demo loop, which is not built.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments

## What is code (GPL-3.0-only)

Every file in this tree that carries an `SPDX-License-Identifier: GPL-3.0-only` header (one source file
carries another: `firmware/src/fm6_core.c`, msfa, Apache-2.0, below; the firmware built with it is
GPL-3.0-only as a whole):

- the firmware: `firmware/` (app, HAL, update loader)
- the build script and tools: `build.sh`, `tools/`
- the installer page, its libraries and their test: `web/`
- the host tests: `tests/`

You may use, study, change and share it under the GPL. If you distribute Felucca, or
firmware derived from it, you must also give your recipients its complete corresponding
source under the same licence. That includes devices that ship with modified Felucca
inside.

## Additional permission (GPL-3.0 section 7)

As an additional permission under GPL-3.0 section 7, you may combine Felucca, or a work
based on it, with the Felucca Assets (above), and convey the combination.
This is allowed even though the Felucca Assets are not licensed under the GPL, provided
that:

- you follow the GPL for every part that is not a Felucca Asset; and
- you follow the terms published for the assets.

The Felucca Assets are data (wavetables, icons, sample data). They are not program
code. A firmware image built from the GPL sources with replacement assets, or with no
assets, is entirely governed by the GPL.

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Instrument and drum samples (Versilian Studios VSCO-2 CE, VCSL; Sonic Pi: SCRCH) | CC0 1.0 | `assets/samples-cc0/`, provenance in `ATTRIBUTION.txt` there |
| Terminus font 8x16 (Terminus Bold, ter-u16b): the FM-1's screen | SIL OFL 1.1 | `assets/fonts/ter-u16b.bdf`, `assets/fonts/Terminus-LICENSE.txt` |
| CrispyZebra by Leo Kuroshita (<https://github.com/hugelton/CrispyZebra>): the PHASE engine's waveforms are a C port of its oscillator | GPL-3.0 | `firmware/src/eng_phase.c` |
| klattsch by Tony Gies (<https://github.com/tgies/klattsch>): design reference for the VOICE (formant) engine; no code copied. Formant data from Klatt (1980) / Hillenbrand et al. (1995) | MIT (klattsch) | credit only |
| msfa by Google Inc. and Pascal Gauthier, from Dexed (<https://github.com/asb2m10/dexed>): the FM6 engine's synthesis (SLOOP 2.4), ported to integer C for Felucca 1.0 by Leo Kuroshita (Dexed itself is GPL-3.0; only msfa is used; the FM6 factory patches are Felucca's own) | Apache-2.0 | `firmware/src/fm6_core.c`, `LICENSES/Apache-2.0-msfa.txt` |
| DaisySP (Electrosmith, Emilie Gillet) and Rings' sympathetic strings (Emilie Gillet): the PHYS engine's models (SLOOP 2.5, from Felucca 1.0, ported to fixed point by Leo Kuroshita) | MIT | `firmware/src/phys_dsp.c`, `firmware/src/phys_symp.c` (the notice is in each file) |
| JieLi AC79 SDK: `uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin` are read from your SDK checkout at build time and placed in the package; no SDK files are in this tree | Apache-2.0 | <https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK> |

## Contributions

Contributions are welcome under GPL-3.0-only. By submitting one, you agree that it may be
combined with the Felucca Assets under the section 7 permission above.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.

"M-VAVE" and "FM-1" are trademarks of their respective owners. Felucca is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners.

## Radio

Felucca never enables the Bluetooth / Wi-Fi radio of the hardware.
