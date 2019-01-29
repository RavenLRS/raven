#!/bin/bash

set -e

export CLANG_FORMAT=clang-format-9
make format-check

XTENSA_ESP32_GCC_URL=https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz
ARM_EABI_NONE_GCC_URL=https://developer.arm.com/-/media/Files/downloads/gnu-rm/7-2018q2/gcc-arm-none-eabi-7-2018-q2-update-linux.tar.bz2

mkdir -p tmp && pushd tmp

curl -L ${XTENSA_ESP32_GCC_URL} | tar xzf -
export PATH=`pwd`/xtensa-esp32-elf/bin:${PATH}

curl -L ${ARM_EABI_NONE_GCC_URL} | tar xjf -
export PATH=`pwd`/gcc-arm-none-eabi-7-2018-q2-update/bin:${PATH}

popd

# Setup targets to build
case $1 in
tx)
    TARGETS_FILTER=_tx
    ;;
rx)
    TARGETS_FILTER=_rx
    ;;
txrx)
    TARGETS_FILTER=_txrx
    ;;
esac

export TARGETS_FILTER
make ci-build
