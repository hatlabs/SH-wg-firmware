# Sailor Hat WiFi Gateway (SH-wg) Firmware

## Introduction

This repository provides firmware for the [Sailor Hat WiFi Gateway](https://github.com/hatlabs/SH-wg-hardware).
SH-wg implements an NMEA 2000 WiFi gateway that broadcasts NMEA 2000 sentences over WiFi.
To perform its function, the device needs to be connected to an NMEA 2000 bus and to a WiFi access point.

SH-wg is available for purchase at [Hat Labs online shop](https://hatlabs.fi/product/sh-wg/).

## Installation

This project uses [PlatformIO](https://platformio.org/) for building and uploading the firmware.
No on-board USB interface is provided; to connect a computer to the SH-wg, you must use a USB-to-serial adapter.
[ESP-Prog](https://docs.espressif.com/projects/espressif-esp-iot-solution/en/latest/hw-reference/ESP-Prog_guide.html) is known to work and easily available.

If you are using ESP-Prog, note that it exposes two serial ports.
The first port is for the JTAG interface and the second one for programming and serial communication.
Depending on your operating system, you may have to configure the serial port on `platformio.ini` lines 46 and 47.

Connect the ESP-Prog to the SH-wg, either by using the exposed 2x3 pin header on the mezzanine board, or by connecting the programmer to the board interconnect pin header.
Then, select "Upload and Monitor" from the PlatformIO "bug" menu.
This will build and upload the firmware and start the serial monitor.

## Documentation

The full SH-wg documentation is available at [docs.hatlabs.fi/sh-wg](https://docs.hatlabs.fi/sh-wg).

## Utilities

The repository includes some Python scripts for monitoring UDP broadcast traffic as well as broadcasting recordings over UDP.

To monitor UDP traffic, run the following command:

```shell
./udp_broadcast_monitor.py 2002
```

where 2002 refers to the port to listen to.
The received data will be printed out on standard output, with newline characters stripped.

To record the data, redirect output to a file:

```shell
./udp_broadcast_monitor.py 2002 > output.txt
```

You can play back recorded data with the following command:

```shell
./udp_broadcast_transmitter.py 2002 data/ydwg_recording_1.txt
```

where 2002 refers to the port to broadcast on, and data/ydwg_recording_1.txt is the file to read from.
Only YDWG RAW data is supported.
