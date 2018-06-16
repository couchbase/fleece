# ESP32 Development

This is the directory for building Fleece for the [ESP32][ESP32] embedded platform with the IDF framework.

The unit tests are broken into two projects (subdirectories `Tests1` and `Tests2`), because otherwise the resulting program is too large to fit in the ESP32's flash storage. To build/run either one, you need to `cd` into its subdirectory.

## Installation

If you haven't already, [set up the ESP-IDF build environment](https://esp-idf.readthedocs.io/en/latest/get-started/index.html):

1. Download [the toolchain](https://dl.espressif.com/dl/xtensa-esp32-elf-osx-1.22.0-80-g6c4433a-5.2.0.tar.gz)
2. Get ESP-IDF API libraries via `git clone --recursive https://github.com/espressif/esp-idf.git`

You'll need to set up some environment variables to point to these:

    $ PATH=$PATH:/path/to/xtensa-esp32-elf/bin
    $ export IDF_PATH=/path/to/esp-idf

## First-Time Setup

Before building either test program the first time, you'll need to change some settings.

First, identify the serial port of your device. Plug it into USB and run `ls /dev/cu.*`. The file whose name contains `usbserial` is the device; e.g. `/dev/cu.usbserial-DN041OIM`. _[This is for macOS; the setup instructions linked above have directions for Linux and Windows.]_

    $ cd Fleece/Make/ESP32/Tests1
    $ make menuconfig
    
This brings up the semi-GUI config utility. Change the serial port setting to the full path of the device.

For ESP32 Thing boards, you _may_ need to change the main clock frequency configuration to 26MHz in menuconfig, or the monitor will display garbage. (This happened to me.)

Once you've configured one of the program directories, you can copy the `sdkconfig` file into the other one:

    $ cp sdkconfig ../Tests2/

## Building

`make`

## Running Tests

`make flash monitor`

[ESP32]: https://www.espressif.com/en/products/hardware/esp32/overview