#!/bin/bash

convert ${1} -monochrome -resize 30x30 raven_%d.xbm[0-100]
for f in *.xbm; do
    # Change char to const char
    sed -i '' "s/static char/static const char/" ${f}
    # Remove trailing whitespace
    sed -i '' 's/^[ \t]*//;s/[ \t]*$//' ${f}
done
