# ESP32 Development

This is the directory for building Fleece for the [ESP32][ESP32] embedded platform with the IDF framework.

## Installation

If you haven't already, [set up the ESP-IDF build environment](https://esp-idf.readthedocs.io/en/latest/get-started/index.html):

1. Download [the toolchain](https://dl.espressif.com/dl/xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz)
2. Get ESP-IDF API libraries via `git clone --recursive https://github.com/espressif/esp-idf.git`

You'll need to set up some environment variables to point to these:

    $ PATH=$PATH:/path/to/xtensa-esp32-elf/bin
    $ export IDF_PATH=/path/to/esp-idf

## First-Time Setup

Before building the first time, you'll need to change some settings.

First, identify the serial port of your device. Plug it into USB and run `ls /dev/cu.*`. The file whose name contains `usbserial` is the device; e.g. `/dev/cu.usbserial-DN041OIM`. _[This is for macOS; the setup instructions linked above have directions for Linux and Windows.]_

    $ cd Fleece/Make/ESP32/
    $ make menuconfig
    
This brings up the semi-GUI config utility. Change the serial port setting to the full path of the device.

For ESP32 Thing boards, you _may_ need to change the main clock frequency configuration to 26MHz in menuconfig, or the monitor will display garbage. (This happened to me.)

## Building

`make`

## Running Tests

`make flash monitor`

[ESP32]: https://www.espressif.com/en/products/hardware/esp32/overview