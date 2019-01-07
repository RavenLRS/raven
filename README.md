# Raven Long Range System

[![Build Status](https://travis-ci.com/RavenLRS/raven.svg?branch=master)](https://travis-ci.com/RavenLRS/raven)

Raven is completely open source a long range system (LRS) based on LoRa
which also supports peer to peer communications (so for example,
flight controllers in aircrafts can talk directly to each other).

# Main features

- Long Range support using LoRa for the main RC link between TX and RX.
- Full telemetry, integrated with the radio (voice alerts with OpenTX, etc...).
- Supports 7 different bands: 147Mhz, 169Mhz, 315Mhz, 433Mhz, 470Mhz, 868Mhz and 915Mhz.
- Up to 20 channels.
- Support for P2P for aircraft to aircraft communication (either direct
or via pilots on the ground). Allows, for example, displaying a radar in the
OSD, automatically choosing a free VTX channel or automatically following
another aircraft.
- OLED screens, both on the TX and RX with channel monitor, telemetry
viewer and full configuration (requires at least one button on the board).
- Bluetooth MSP bridge using the same serial port as the RC link (works with
all the existing Betaflight/iNAV configurators with support for Bluetooth).
- Support for OTA updates over Bluetooth (iOS and Android apps coming soon).
- Model ID with 64 memory slots.
- Fully configurable from the radio using CRSF scripts (crossfire.lua
and device.lua).
- Low latency. 250Hz between radio and TX as well as between TX and flight
controller. Air protocol is limited to 100Hz for now, but will support up
to 150hz with telemetry or 200hz without telemetry in the near future.
- Multiple RX protocols supported (SBUS+SmartPort, FPort, MSP, CRSF, ...).
- Support for backup batteries (useful for missing aircraft recovery).

![Raven TX on a Q X7](docs/images/raven_qx7.png?raw=true "Raven TX on a Q X7")

## Compiling Raven

Raven is built on top of [esp-idf](https://github.com/espressif/esp-idf), but it includes it
as a submodule, so you should only need to install the [Xtensa toolchain](https://docs.espressif.com/projects/esp-idf/en/stable/get-started/index.html)

Raven you should install and configure it. Make sure to install version 3.0 by running
`git checkout v3.0` (their instructions will give you the development version, not a stable one).
Download the required submodules by running `git submodule init` followed by `git submodule update -r` in the same directory that you have cloned the esp-idf repository (i.e. no need to clone the master esp-idf repository).

Then, clone the Raven repository by running `git clone --recursive https://github.com/RavenLRS/raven.git`. Don't forget the `--recursive` option, since Raven
uses submodules.

From the directory where you've cloned Raven, run `PORT=<port> TARGET=<target> make erase flash` to flash a new module. For
updating a board which is already running Raven, omit the `erase` part to avoid wiping your configuration. The `erase` is only
needed for new boards, since they might come with some pre-flashed app that can interfere with Raven.

Run `make` without any arguments to print the help, which includes additional instructions about the port naming as well as the
valid list of targets. Targets use the following naming convention:

- [board-name]_tx: Raven with support for working as TX only (connected to the radio).
- [board-name]_rx: Raven with support for working as RX only (connected to the flight controller or the servos/ESCs).
- [board-name]_txrx: Raven with support for both TX and RX, controlled by an option. Note that type of build is mostly used
for development and troubleshooting. Most of the time you should flash the TX or the RX variants.

If you want to see the debug logs, you can use the builtin esp-idf monitor by running `PORT=<port> TARGET=<target> make monitor`.


## Hardware setup

A typical setup of 100mw TX and RX for 433Mhz or 868/916Mhz costs $20-30. All popular ESP32 boards with LoRa are supported and can be used as both TX and RX.

## Getting started

Check [the documentation](docs/getting_started.md) to get up and running in no time!