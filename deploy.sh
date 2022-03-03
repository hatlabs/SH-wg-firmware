#!/usr/bin/env bash

set -euo pipefail

firmware_file=.pio/build/esp32dev/firmware.bin
server=mo.lan
website_path=/lv00/Data/websites/ota.hatlabs.fi

# extract the firmware name from the firmware_info.h file
firmware_name=$(sed -nr '/kFirmwareName/ s/.*kFirmwareName = "([^"]+)".*/\1/p' src/firmware_info.h)

# read the latest hex version
hex_version=$(cat latest)

# build the project

pio run --environment esp32dev --target clean
pio run --environment esp32dev

# copy the built binary to the server

rsync -av $firmware_file $server:$website_path/$firmware_name/firmware_${hex_version}.bin

# update the version indicator

rsync -av latest $server:$website_path/$firmware_name/latest
