# Raven Long Range System

Raven is completely open source a long range system (LRS) which runs on inexpensive ESP32 and uses LoRa for the link layer.

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
