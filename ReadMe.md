# Kiisu Firmware

Fork of [Flipper Zero official firmware](https://github.com/flipperdevices/flipperzero-firmware) with small fixes for [Kiisu board](https://kiisu.io).

## Inclided Apps from https://github.com/twoelw
* Kiisu Companion Bridge for Aux MCU flash (https://github.com/twoelw/kiisu-companion-bridge)
* Kiisu Sensor Hub (https://github.com/twoelw/kiisu-sensor-hub)
* Kiisu Manager (https://github.com/twoelw/kiisu-manager)

## Flash via Flipper Lab
1. Install SD Card into Kiisu and format it, connect your Kiisu board via USB
2. Use this link: [https://lab.flipper.net/?url=https://github.com/kiisu-io/kiisu-firmware/releases/download/v1.1/kiisu-z-f7-update-local.tgz&target=f7&channel=release-cfw&version=kiisu-v1.1](https://lab.flipper.net/?url=https://oksa.ee/kiisu11.tgz&target=f7&channel=release-cfw&version=kiisu-11)
3. Press Connect (if not connected automatically)
4. Press Install

## Flash via qFlipper
1. Download qFlipper here: https://flipperzero.one/downloads
2. Download latest firmware from Releases - you need kiisu-z-f7-update-local.tgz file
3. Connect your Kiisu board to the PC
4. Use Install from file button

## Building

Build firmware using Flipper Build Tool:

```shell
./fbt
```
Build firmware .tgz bundle (for qFlipper):

```shell
./fbt COMPACT=1 DEBUG=0 updater_package
```

