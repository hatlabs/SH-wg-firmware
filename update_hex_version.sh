#!/usr/bin/env bash

set -euo pipefail

if [[ $(git diff --stat) != '' ]]; then
    echo 'Working directory is not clean. Aborting.'
    exit 1
fi

version=$(cat VERSION)
hex_version=$(cat VERSION | ./get_hex_version.py)

sed -i -E "s/kFirmwareHexVersion = \(.*\)\;/kFirmwareHexVersion = 0x$hex_version;/" src/firmware_info.h

git add src/firmware_info.h
git commit -m "Update hex version value to $hex_version"

echo "$hex_version" > latest
