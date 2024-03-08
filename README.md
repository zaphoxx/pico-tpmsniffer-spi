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

* Connect DI (MOSI), DO (MISO), CLK (clock), #CS (select) to GPIO 2,3,4,5 of the PICO! 
* Connect GND Pico with GND of the BIOS Chip!
* Connect pico to serial terminal e.g., minicom
* Boot laptop and loot

!Hint! Connect pico to serial (e.g., minicom) ! Press reset on pico and within the 5second countdown
start the laptop. If you start the laptop after the 5 second countdown the tpm key will not be captured.
This is something i am still looking into and need to figure out how to fix that!

## Description

The sniffer collects MISO and MOSI data in parallel. It will search the MOSI line for the "Read FIFO_0"
command which corresponds to the byte sequence 0x8XD40024 (X holds the number of bytes the TPM is
expected to transfer on the MISO line). It will then search the MISO data response for the VMK header
and if successfully identified will output the VMK.

The sniffer was tested on a Lenovo T460 with TPM1.2. On the laptop i had the actual TPM 
communication starts about 27 seconds about the same time the splash screen shows up on
the screen. About 5-10 seconds later you should get the key result.   
I also tested on another Lenovo T460 which is a slightly different model. There the key is captured 
~9 seconds after starting the boot process.
