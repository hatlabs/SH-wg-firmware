# Sailor Hat WiFi Gateway (SH-wg) Firmware

## Introduction

This repository provides firmware for the [Sailor Hat WiFi Gateway](https://github.com/hatlabs/SH-wg-hardware).
SH-wg implements an NMEA 2000 WiFi gateway that broadcasts NMEA 2000 sentences over WiFi.
To perform its function, the device needs to be connected to an NMEA 2000 bus and to a WiFi access point.

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

## Description of LEDs

The SH-wg device includes a number of LEDs that indicate the status of the device.

<img src="media/led_numbers.jpg" width="50%"/>

1. Red: Power LED.
   On when the device is powered.
   Turns off when magnet presence is detected (see the next section).
   Blinks rapidly during an over-the-air update.
2. Blue: WiFi LED.
   On when WiFi is connected.
   Blinks when the configuration portal is activated.
3. Yellow: Normally on.
   Blips when data is broadcast over UDP.
4. Green: TX: Blinks when data is transmitted over the NMEA 2000 bus.
5. Green: RX: Blinks when data is received over the NMEA 2000 bus.

## Magnet "Button" User Interface

SH-wg is enclosed in a waterproof enclosure.
Basic interaction such as restarting and resetting the device is nevertheless possible using a magnet.
Slide the provided neodymium magnet against the enclosure next to the magnet sensor shown in the picture below.

<img src="media/hall_effect_sensor.jpg" width="50%"/>

When the magnet is detected, the red LED turns off.

The sensor reacts to two different events:

- One second "button press": restart the device.
- Ten second long "button press": reset the device to factory defaults.
  This can be used to re-enable the configuration portal if the device no longer connects to the WiFi.

## Initial Configuration

An unconfigured SH-wg will create its own WiFi access point (a so-called "configuration portal).
The user can connect to that access point and define the WiFi configuration.

When the configuration portal is activated, the blue LED is blinking.
The device is visible on the computer's WiFi network listing:

<img src="media/wifi_selection.jpg" width="40%" />

The network name is "Configure sh-wg-xxxxxxxxxxxx", the last 12 digits corresponding to the device unique identifier.
You also need to provide a password to connect to the configuration portal. The password is "abcdabcd".

Once you have successfully connected to the configuration portal, you should be automatically presented with the WiFi configuration front page:

<img src="media/captive_portal_front_page.jpg" width="70%" />

Click the "Configure WiFi" button.
You'll get a list of nearby WiFi networks.

<img src="media/wifi_configuration.jpg" width="70%" />

One of them should be the boat network you want to connect to.
Select that and enter the network password.
Press "save".

If the SH-wg is able to connect to the configured network, it will restart itself and the blue LED will stop blinking and turn on.
If the configuration failed, the blue LED will keep blinking and you must repeat the operation.

## OTA Updates

SH-wg will periodically check an online server for updates.
If an update is available, it will be automatically downloaded and installed.
Once the update is finished, the device will resume operating with the old firmware.
Only once the device is restarted will it switch to the new firmware version.

## Operation

SH-wg receives NMEA 2000 packets and broadcasts them over WiFi.
Both TCP and UDP traffic is supported, with two different data formats.
The different data formats are described in the following sections.

When implementing a client, the YDWG RAW format over UDP is recommended.
The data encoding is generic and UDP as a transmission protocol is resilient and simple to implement.
Due to the repetitive nature of the data stream, a low level of lost packets is not an issue for transport reliability.

### YDWG RAW Format

The YDWG RAW format is a data link layer raw encoding format for extended CAN frames used to transmit raw NMEA 2000 data.

The format is very simple, consisting of a single CAN frame per line.
The message consists of a timestamp, a one-letter direction of the message ("R" or "T"), the 29-bit extended CAN message identifier, and 1 to 8 message data bytes in hexadecimal format.

The messages are transmitted as-is, without any filtering or interpretation.
It is up to the recipient to detect and decode higher-level transport protocols such as the J1939 Transport Protocol (TP) or the NMEA 2000 Fast Packet Protocol (FP).

An example of a few YDWG RAW messages is shown below:

```text
16:32:53.030 R 08FF0000 13 99 04 05 00 00 02 00
16:32:53.004 R 1CFF2102 13 99 84 00 FF FF FF FF
16:32:53.009 R 09F80101 FF FF FF 7F FF FF FF 7F
16:32:53.012 R 15FD0602 FF FF FF FF FF FF FF FF
```

The full protocol specification is provided in [Yacht Devices YDWG-02 User Manual](https://www.yachtd.com/downloads/ydwg02.pdf), Appendix E.

YDWG Raw traffic is provided over both TCP and UDP.
SH-wg YDWG RAW server listens on port 2223.
UDP packets are respectively broadcast on port 2002.

### SeaSmart Format

The SeaSmart (also known as SeaSmart.Net) format encapsulates NMEA 2000 data in NMEA 0183 sentences.
It is a transport layer format, meaning that the CAN identifiers as well as recognized J1939 TP and NMEA 2000 Fast Packets are already decoded.

An example of the SeaSmart format is shown below:

```text
$PCDIN,00FF00,03FEA696,00,1399040500000200*29
$PCDIN,01FD06,03FEA6AF,02,FFFFFFFFFFFFFFFF*27
$PCDIN,01FD07,03FEA6B2,02,FFFFFFFFFF7FFFFF*20
$PCDIN,01FD08,03FEA6B5,02,570000FFFFFFFFFF*5B
$PCDIN,01F801,03FEA67E,01,FFFFFF7FFFFFFF7F*2A
$PCDIN,00FF00,03FEA696,00,1399040500000200*29
```

The protocol is specified in [SeaSmart.Net Protocol Specification](http://www.seasmart.net/pdf/SeaSmart_HTTP_Protocol_RevG_043012.pdf).

SH-wg implements a TCP server for SeaSmart traffic on port 2222.
SeaSmart traffic is broadcast over UDP on port 2000.

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
