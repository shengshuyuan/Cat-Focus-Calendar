<p align="right">
  <a href="protected-flash-layout.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Protected Flash Layout

This repository reserves the per-device identity region so derivative firmware
does not overwrite provisioned `cardid` data.

## Mandatory layout

Derivative projects must preserve all of the following:

- ESP32-C3, 8 MB Flash, ESP-IDF 5.5.3.
- A merged ESP image starting at `0x0`, produced as
  `build/FoloToy-AI-Passport-full.bin`.
- One main application image at `0x10000`, no larger than `0x300000` bytes.
- `cardid`: data/NVS at `0x356000`, size `0x4000`.
- A valid partition-table MD5 marker and no partition overlap with the
  protected `cardid` region.
- No device-specific `cardid` payload in a community artifact.

Applications may add resource partitions, but they must not overlap protected
`cardid`. Required resource partitions must be included in the merged
artifact rather than declared empty.

## Enforced validation

Run:

```bash
./tools/validate.sh --firmware
```

The check builds in an isolated directory, creates the merged image, verifies
the bootloader/table/application offsets, parses the partition table, checks its
MD5 and the protected `cardid` range, enforces the 3 MB application limit, and
rejects device-specific identity bytes. CI runs the same gate. Do not publish
an artifact when this command fails.

Upload only `build/FoloToy-AI-Passport-full.bin`; the similarly named app-only
`build/FoloToy-AI-Passport.bin` does not contain the complete validated layout.

## Flashing safety during development

Never run `idf.py erase-flash` on a provisioned device. It destroys the
per-device identity. Prefer segmented `idf.py flash`, which does not write an
image for protected `cardid`. A
raw single-file write from `0x0` is safe only when its byte range ends before
`cardid`; a merged artifact containing later resource partitions spans the gap
and must not be raw-flashed to a provisioned device.

## Daily iteration: factory / app-only flash (preferred)

On a provisioned device, flash **only the application image** when validating UI or app changes. Do not rewrite the bootloader, partition table, or `cardid`:

```bash
. ~/esp/esp-idf-v5.5.3/export.sh
idf.py build
python -m esptool --chip esp32c3 -p /dev/cu.usbmodem101 -b 460800 \
  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x10000 build/FoloToy-AI-Passport.bin
```

- Adjust the serial port for your machine (`/dev/cu.usbmodem*` is common on macOS).
- This writes the app at `0x10000` and stays clear of `cardid@0x356000`.
- Require an explicit flash OK from the user; default to software changes plus `./tools/validate.sh --static` and `idf.py build` first.
