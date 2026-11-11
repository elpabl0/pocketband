# Building Pocket Band

The build makes these files in `build/`:

| File | What |
| --- | --- |
| `pocketband.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `pocketband.fwsc` | the installable package (app + loader) |
| `pocket-band-installer.html` | the package inside a page that installs it from Chrome or Edge |

## What you need

- Python 3 with Pillow: `pip3 install Pillow`
- The JieLi Linux toolchain (clang 4.0.1 for pi32v2, from JieLi's package server):

  ```
  sh tools/get_toolchain.sh            # installs to ~/.jieli/toolchain
  ```

- The JieLi AC79 SDK (Apache-2.0). The package uses three of its files
  (`cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`); they are not part of this tree.

  ```
  git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
      https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
  ```

- Node.js (optional, for the web test).

The toolchain is Linux x86-64 only. On Linux x86-64 it runs natively. Elsewhere (macOS, or with
`JIELI_DOCKER=1`) the build runs each tool in a `linux/amd64` `debian:bookworm-slim` container, so Docker
must be running and the source tree must be in a folder Docker can share.

## Build

```
sh build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations (`~/.jieli/toolchain`,
`~/fw-AC79_AIoT_SDK`).

The version is the string in `firmware/src/pb_core.c` (`"POCKET BAND X.Y"`, one digit each). The package
identity follows from it: `FM-1_7226XY`.

Build options are the `FELUCCA_*` defines at the top of `firmware/src/felucca.c` (flash storage, update
entry, USB audio, TRS MIDI, the second core). `FELUCCA_OTA`, `FELUCCA_FLASH`, `FELUCCA_UART`,
`FELUCCA_CPU1` and `FELUCCA_SLICE` can be set to `0` or `1` in the environment.

## Samples

The CC0 instrument samples of the SAMPLE engine are in `assets/samples-cc0/` (see `ATTRIBUTION.txt`
there). `tools/fetch_cc0.py` downloads the sources again. Without that folder the build still works and
the SAMPLE engine has only the generated drum kit.

## Tests

```
sh tests/run_tests.sh
```

Run it after `sh build.sh` (it uses `build/` and needs `AC79_SDK` set as for the build). It takes a long
time: half an hour or more. It runs Pocket Band's own tests (`tests/pb_brain_test.c`, `tests/pb_fw_test.c`,
the second also under AddressSanitizer and UBSan) and the host tests inherited from SLOOP: flash storage,
the MIDI parser, the update entry and loader, the sequencer, every engine, the command-line installer and,
with Node.js, the update protocol in JavaScript.

The regression suite (`tests/regress.c`) renders every engine and preset and compares a hash of each
render with `tests/golden.txt`; it also checks levels, voices and the CPU cost. After an intended change
of the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites the hashes; `BUDGET_UPDATE=1` does the same
for the cost files.

Known failure: "regression: target cost of the render loops" reports four engines over the static
instruction budget. SLOOP 2.8.5 fails the same check, unmodified, with this toolchain.

## Install from the command line

```
pip3 install mido python-rtmidi
python3 tools/fm1_install.py build/pocketband.fwsc
```

## Releasing

1. Set the version in `firmware/src/pb_core.c`, add it to `CHANGELOG.md`, build, run the tests.
2. Install the build on a real FM-1 and go through [DEMO.md](DEMO.md).
3. Tag it (`v0.4`) and publish a GitHub release with `build/pocketband.fwsc` and
   `build/pocket-band-installer.html` attached. Keep those two file names from release to release.

The Firmware Hub reads `fmhub.json` in the repository root (the card's text, picture and colour) and
picks up each new release by itself. Its rules: <https://thefirmwarehub.com/submit/>.
