# Laptop Trackpad Reuse Project

This project repurposes a laptop Synaptics PS/2 trackpad into a standalone USB HID trackpad using a Raspberry Pi Pico (RP2040) or ESP32-S2. It features a Nokia 105 (128x160) LCD display that visualizes the trackpad touches in real-time.

## Hardware Used
- **Microcontroller**: Raspberry Pi Pico (or ESP32-S2 Mini)
- **Trackpad**: Synaptics PS/2 Trackpad
- **Display**: Nokia 105 LCD (128x160 pixels, SPI)

## Pin Wiring (Raspberry Pi Pico)
| Component | Pin Function | Pico Pin |
|-----------|--------------|----------|
| Trackpad  | PS2 Clock    | GPIO 10  |
| Trackpad  | PS2 Data     | GPIO 11  |
| Display   | SCK          | GPIO 18  |
| Display   | MOSI/TX      | GPIO 19  |
| Display   | RST          | GPIO 16  |
| Display   | CS           | GPIO 17  |
| Button    | Left Click   | GPIO 15  |

## Software Architecture
The firmware is designed to be highly portable and purely **single-core**, allowing it to be compiled and run seamlessly on the RP2040 (Pico) and the single-core **ESP32-S2 Mini**.

- **Interrupt-Driven PS/2**: The trackpad communicates via PS/2 using `attachInterrupt`. A large circular buffer holds incoming bits to ensure no data is lost even when the main loop is blocked by SPI display updates.
- **Absolute W-Mode Processing**: The trackpad is initialized into Synaptics Absolute W-Mode, which transmits exact X/Y coordinates and pressure (Z) values.
- **Native 2-Finger Support**: Complex multi-touch parsing allows for perfect 2-finger scrolling without any jitter or skipping.
- **TinyUSB HID**: The microcontroller acts as a composite USB device using the Adafruit TinyUSB library, reporting standard mouse movements and clicks.

## Future Plans
- Adding configurable DPI.
