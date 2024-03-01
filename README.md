# Pico TPMSniffer

> This is experimental software and hardware. It's not ready to use for professional or production use.

This is a variation for SPI of the original TPMSniffer from Thomas 'stacksmashing' Roth - code@stacksmashing.net
At the current state this is not compatible with the original hardware design (I removed the hardware/ folder from
this repo to avoid confusion).

## Building

```
export PICO_SDK_PATH=path to your Pico-SDK
mkdir build
cd build
cmake ..
make
```

## Hardware

For easier adhoc connection to the BIOS Chip I used a SOIC8 SOP8 test clip.

## Usage

### SPI-TPM-SNIFFING:
Prerequisites

BIOS-Chip and TPM-Chip use the same SPI-Bus

* Connect SO (MISO), CLK (clock), SCK (select) to GPIO 2,3,4 of the PICO! 
* Connect GND Pico with GND of the BIOS Chip!
* Connect pico to serial terminal e.g., minicom
* Boot laptop and loot
