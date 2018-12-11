#!/bin/bash

set -e

XTENSA_ESP32_GCC_URL=https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz

mkdir -p tmp && pushd tmp
curl ${XTENSA_ESP32_GCC_URL} | tar xzf -
export PATH=`pwd`/xtensa-esp32-elf/bin:${PATH}
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
