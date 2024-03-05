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

For easier adhoc connection to the BIOS Chip I used a SOIC8 SOP8 test clip. You might need
to add a high value pull-up resistor (e.g, 220kOhm) connecting the SCK pin with the 3.3V out
from the pico.

## Usage

### SPI-TPM-SNIFFING:
Prerequisites

BIOS-Chip and TPM-Chip use the same SPI-Bus

* Connect DI, SO (MISO), CLK (clock), SCK (select) to GPIO 2,3,4,5 of the PICO! 
* Connect GND Pico with GND of the BIOS Chip!
* Connect pico to serial terminal e.g., minicom
* Boot laptop and loot

## Description

The sniffer collects the data bytes from SO line looking for response signature 0x80000001.
It then searches the VMK header and once found will output the 32 byte long key.

In this version of the sniffer it does not make use of the SI line to search for the actual
TPM read command.

The sniffer was tested on a Lenovo T460 with TPM1.2. On the laptop i had the actual TPM 
communication starts about 27 seconds about the same time the splash screen shows up on
the screen. About 5-10 seconds later you should get the key result.   
