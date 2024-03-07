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

* Connect DI (MOSI), DO (MISO), CLK (clock), SCK (select) to GPIO 2,3,4,5 of the PICO! 
* Connect GND Pico with GND of the BIOS Chip!
* Connect pico to serial terminal e.g., minicom
* Boot laptop and loot

## Description

The sniffer searches for the "Read Fifo_0" signature 0x8XD40024 where X indicates
the number of bytes the TPM is expected to transfer in response to this read request.
It then searches the VMK header and once found will output the 32 byte long key.

The sniffer was tested on a Lenovo T460 with TPM1.2. On the laptop i had the actual TPM 
communication starts about 27 seconds about the same time the splash screen shows up on
the screen. About 5-10 seconds later you should get the key result.
On another Lenovo T460 (different mainboard version) the TPM communication starts much sooner
(~9seconds after pushing the button).
