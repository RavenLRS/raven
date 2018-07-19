#!/bin/bash

set -e -x

ESP_IDF_REPO_BASENAME=esp-idf
ESP_IDF_REPO=https://github.com/espressif/${ESP_IDF_REPO_BASENAME}.git
ESP_IDF_TAG=v3.0
XTENSA_ESP32_GCC_URL=https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-80-g6c4433a-5.2.0.tar.gz

mkdir -p tmp && pushd tmp
curl ${XTENSA_ESP32_GCC_URL} | tar xzf -
export PATH=`pwd`/xtensa-esp32-elf/bin:${PATH}
git clone ${ESP_IDF_REPO} && pushd ${ESP_IDF_REPO_BASENAME}
git checkout ${ESP_IDF_TAG} && git submodule init && git submodule update
export IDF_PATH=`pwd`
popd
popd

# Build all targets
make ci-build