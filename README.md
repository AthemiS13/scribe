# Scribe (Hardware)

[**▶ Watch the Scribe 2025 Demo on YouTube**](https://youtu.be/QY42d00tYvc)

[![Watch the video](https://img.youtube.com/vi/QY42d00tYvc/maxresdefault.jpg)](https://youtu.be/QY42d00tYvc)

## Overview

Scribe is a compact device designed to enhance productivity and memory retention. With a slim width of just 12mm, it incorporates a custom PCB for a sleek and efficient design. This device features an OLED screen, Bluetooth connectivity for seamless communication with PC, and a rechargeable LiPo battery with built-in protection and charging circuit.

This repository contains the firmware and hardware design files for the Scribe device.

**Companion App:** [Scribe App](../scribe-app/README.md) - The software interface for sending data to Scribe.

## Features

*   **Core:** ESP32-S3 Microcontroller for powerful processing and connectivity.
*   **Display:** 0.91 Inch OLED (128x32) for clear text display.
*   **Connectivity:** 
    *   Bluetooth Low Energy (BLE) for communication with the Scribe App.
    *   WiFi capabilities for AI integration.
*   **AI Integration:** Features AI-powered functions (e.g., Gemini integration) for enhanced utility.
*   **Power:** 
    *   Rechargeable LiPo battery.
    *   Built-in charging and protection circuit.
    *   Battery voltage monitoring.
*   **Input:** Physical system button for navigation and control.

## Application

The primary function of Scribe is to assist with memory and organization. Users can send text strings via the [Scribe App](../scribe-app/README.md), which the Scribe firmware processes and organizes into pages. This helps users keep track of important notes, dates, and names, enhancing their ability to recall information easily.

## Hardware & Schematic

The schematic for Scribe outlines a compact and efficient design, featuring an ESP32-S3 microcontroller for core processing and Bluetooth connectivity. It includes a LiPo battery charging and protection circuit, a 3.3V regulator for stable power, and connections for an OLED display and system button. Key components are protected with ESD diodes and filtering capacitors, ensuring reliable operation and robust performance.

### PCB
The custom 12mm PCB is over 60% smaller than the V1 prototype.
*   **PCB Design:** Available on [OSHW Lab](https://oshwlab.com/atemis/smartpen)

![PCB Front](Hardware/Screenshot%202024-11-30%20160039.png)
![PCB Back](Hardware/Screenshot%202024-11-30%20160101.png)
![PCB Layout](Hardware/Screenshot%202024-11-30%20160855.png)

### Pinout

| Name | Pin | Description |
| :--- | :--- | :--- |
| **SCL** | GPIO 4 | OLED Clock |
| **SDA** | GPIO 5 | OLED Data |
| **BTN** | GPIO 8 | System Button (Pulldown) |
| **BAT** | GPIO 2 | Battery Voltage Monitoring |

*Note: The cartridge touch pin functionality was discontinued to utilize the pin (GPIO 2) for accurate battery voltage monitoring, correcting an issue in the initial design.*

## Development

The firmware is developed using PlatformIO and Arduino framework.

### Arduino Setup
**Important:** Since Scribe uses a custom-built board, a specific Arduino setup is required for the ESP32-S3 features to function correctly. Please refer to the settings in the image below:

![Arduino Setup](Hardware/Screenshot%202024-12-20%20213204.png)

### Structure
*   `src/`: Source code for the firmware.
    *   `scribe.ino`: Main firmware logic for page display and navigation.
    *   `ai.ino`: AI features and WiFi connectivity.
*   `platformio.ini`: Project configuration.

## Credits

*   **Hardware Design & Firmware:** [AthemiS13](https://github.com/AthemiS13)
*   **App Backend:** [George](https://github.com/freddycz)
*   **Sponsor:** [OSHW LAB](https://oshwlab.com/)

![OSHW Sponsor](Hardware/oshw.png)
