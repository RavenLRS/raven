#!/bin/bash

SRC_DIRS="main components/hal-common components/hal-esp32 components/hal-stm32"
FILES=$(find ${SRC_DIRS} -type f \( -iname '*.c' -o -iname '*.h' -o -iname '*.cc' -o -iname '*.cpp' \))
VERBOSE=""
CLANG_FORMAT=${CLANG_FORMAT:-clang-format} 

if [ x${V} == x1 ]; then
    VERBOSE="-verbose"
fi

case "$1" in
format)
    for file in ${FILES}; do
        ${CLANG_FORMAT} ${VERBOSE} -i --style=file ${file}
    done
    ;;
check)
    for file in ${FILES}; do
        if [ ! -z ${VERBOSE} ]; then
            echo "Checking ${file}"
        fi
        if ${CLANG_FORMAT} --style=file -output-replacements-xml ${file} | grep -c "</replacement>" > /dev/null; then
            echo "File ${file} is not correctly formatted" 1>&2
            ${CLANG_FORMAT} --style=file ${file} | diff -u ${file} -
            exit 1;
        fi
    done
    ;;
*)
    echo "Invalid subcommand \"${1}\"" 1>&2
esac

