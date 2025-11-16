# Samsung Snap Killer

Automatically kills Samsung's `vendor.samsung.hardware.snap-service` to prevent battery drain and device slowdowns.

## Installation

Download the latest module from [Releases](../../releases) and flash in Magisk/KSU.

## Building

```bash
make              # Build binary only
make module       # Build Magisk/KSU flashable zip
make clean        # Clean build artifacts
```

Requires `aarch64-linux-gnu-gcc` cross-compiler and `zip` utility. Can also build on-device with `make native` if you have a native toolchain.

## Details

Uses inotify to monitor `/proc` for the snap-service and kills it immediately. Very low CPU usage (~5% of 1 core during periodic polling). Statically linked, runs at nice priority 19.
