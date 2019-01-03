#!/usr/bin/env python

from __future__ import print_function
from __future__ import division

import argparse
import json
import os.path
import zipfile

METADATA_JSON_FILENAME = 'metadata.json'
PARTITIONS_CSV_FILENAME = 'partitions.csv'

def build_release(args):
    data = dict(v=1, esptool_write_flash_options=args.esptool_write_flash_options)
    data['esptool_write_flash_options'] = args.esptool_write_flash_options
    data['partitions'] = PARTITIONS_CSV_FILENAME
    files = []
    output = os.path.join(args.output_dir, args.basename + '.esp32')
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as f:
        addr = None
        for item in args.esptool_all_flash_args.split(' '):
            if item.startswith('0x'):
                if addr is not None:
                    raise ValueError('unexpected address')
                addr = int(item, 16)
            else:
                if addr is None:
                    raise ValueError('unexpected filename')
                basename = os.path.basename(item)
                files.append(dict(addr=addr, file=basename))
                addr = None
                with open(item) as bf:
                    f.writestr(basename, bf.read())
        data['files'] = files
        f.writestr(METADATA_JSON_FILENAME, json.dumps(data), zipfile.ZIP_DEFLATED)
        with open(args.partitions_csv) as p:
            f.writestr(PARTITIONS_CSV_FILENAME, p.read())

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--esptool-write-flash-options')
    parser.add_argument('--esptool-all-flash-args')
    parser.add_argument('--partitions-csv')
    parser.add_argument('--basename')
    parser.add_argument('--output-dir')
    args = parser.parse_args()
    build_release(args)

if __name__ == '__main__':
    main()