# Raven Long Range System

Raven is completely open source a long range system (LRS) based on LoRa
which also supports peer to peer communications (so for example, 
flight controllers in aircrafts can talk directly to each other).

# Main features

- Long Range support using LoRa for the main RC link between TX and RX.
- Full telemetry, integrated with the radio (voice alerts with OpenTX, etc...).
- Up to 20 channels.
- Support for P2P for aircraft to aircraft communication (either direct 
or via pilots on the ground). Allows, for example, displaying a radar in the
OSD, automatically choosing a free VTX channel or automatically following
another aircraft.
- OLED screens, both on the TX and RX with channel monitor, telemetry 
viewer and full configuration (requires at least one button on the board).
- Bluetooh MSP bridge using the same serial port as the RC link (works with
all the existing Betaflight/iNAV configurators with support for Bluetooth).
- Model ID with 64 memory slots.
- Fully configurable from the radio using CRSF scripts (crossfire.lua
and device.lua).
- Low latency. 250Hz between radio and TX as well as between TX and flight
controller. Air protocol is limited to 100Hz for now, but will support up
to 150hz with telemetry or 200hz without telemetry in the near future.
- Multiple RX protocols supported (SBUS+SmartPort, CRSF, MSP, CRSF, ...).
- Support for backup batteries (useful for missing aircraft recovery).

![Raven TX on a Q X7](docs/images/raven_qx7.png?raw=true "Raven TX on a Q X7")

## Building Raven

Raven is built on top of [esp-idf](https://github.com/espressif/esp-idf), so as a first step to build raven you should install
and configure it.

Make sure all the requires submodules have been checked out by running `git submodule init` followed by `git submodule update`.

Then, use `make menuconfig` to configure how you want to build Raven. Raven specific options (board, frequencies, TX or RX support, etc...)
are found under `Components -> Raven LRS`. Note that there ESP related options that you might want to adjust (for example, the serial port
used to communicate with the board).

Finally, type `make flash` to flash and reboot the board. If you want to see the debug logs, you can use the builtin esp-idf monitor by
running `make monitor`.


## Hardware setup

A typical setup of 100mw TX and RX for 433Mhz or 868/916Mhz costs $20-30. All popular ESP32 boards with LoRa are supported and can
be used as both TX and RX.
