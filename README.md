# Synaptics PS/2 Trackpad to USB HID Adapter (Raspberry Pi Pico)

This project uses a Raspberry Pi Pico (RP2040) to convert an old laptop Synaptics PS/2 trackpad into a standard USB mouse. It supports multi-finger gestures like two-finger scrolling and uses a high-resolution sub-pixel tracking algorithm to guarantee perfectly smooth cursor movement.

Additionally, this project drives an ST7735 SPI display to visualize the touch data in real-time, showing coordinates and multi-touch states.

## Hardware Required
1. Raspberry Pi Pico (RP2040)
2. Synaptics PS/2 Trackpad (extracted from an old laptop)
3. ST7735 SPI Display (Optional, for visual debugging)
4. Push Button (for left click)

## Pinout
- **PS/2 Clock:** GPIO 10
- **PS/2 Data:** GPIO 11
- **Left Click Button:** GPIO 15
- **ST7735 SPI SCK:** GPIO 18
- **ST7735 SPI TX (MOSI):** GPIO 19
- **ST7735 SPI CS:** GPIO 17
- **ST7735 SPI DC:** GPIO 16
- **ST7735 SPI RST:** GPIO 20

## Software Setup
1. Install the Arduino IDE.
2. Install the **Earle F. Philhower RP2040 core** in the Boards Manager.
3. Install the **Adafruit ST7735 and ST7789 Library** and **Adafruit GFX Library**.
4. In the `Tools` menu, set the USB Stack to **"Adafruit TinyUSB"**.
5. Compile and flash the Pico.

## Features
- **High Resolution Tracking:** Reads the raw 13-bit (5000x5000) absolute coordinate data from the Synaptics sensor and accumulates fractional movements so no sub-pixel data is lost.
- **Two-Finger Scroll:** Detects multi-touch state and translates physical vertical drag into standard USB scroll wheel events.
- **Visualizer:** Real-time visual heat map of your finger position on the LCD display.
