![Scribe-banner](https://github.com/freddycz/smart-pen/blob/main/Hardware/Banner.png)

# Update 2025: 
## PCB for SmartPen available here: 
https://oshwlab.com/atemis/smartpen

![PCB-FRONT](https://github.com/freddycz/smart-pen/blob/main/Hardware/Screenshot%202024-11-30%20160039.png)

![PCB-BACk](https://github.com/freddycz/smart-pen/blob/main/Hardware/Screenshot%202024-11-30%20160101.png)

![PCB-LAYOUT](https://github.com/freddycz/smart-pen/blob/main/Hardware/Screenshot%202024-11-30%20160855.png)

1. After 2 months of 24/7 grind, I was able to get SmartPen to work. I would like to thank [OSHW LAB](https://oshwlab.com/ "OSHW") for sponsoring me. Without them, it would be extremely difficult to complete this project because of its very high cost.
2. SmartPen now has a custom 12 mm PCB (more than 60% smaller than V1) and many other features including battery status indicator, lipo protection, in-hand detection, reworked main switch, and more
3. I will update the .md of this project once my colleague [George](https://github.com/freddycz "George") finishes the user interface.
4. 3D design is still not done yet since I have a lot of work.

# SmartPen

SmartPen is a compact device designed to enhance productivity and memory retention

## Wiring

| Name | Pin  | Desctiption  |
| ------------ | ------------ | ------------ |
| SCL  | GPIO4  |  OLED |
| SDA  |  GPIO5 | OLED  |
| CTGSW  | GPIO8  | Cartridge BTN  Pulldown |
| BATADC  | GPIO33  | Battery Voltage  |
| SYSSW  | GPIO37  | System BTN  Pulldown|
| TCH | GPIO2  | Touch Sensor  |

## Features

**Core:** ESP32-S3FN8 Processor with integrated 8MB Flash Memory    
**Communication:** Bluetooth, Wifi and Serial USB-C    
**Power management:** Single cell Li-Po 3.7v Battery charging circuit with 0.6A Charging current and overcharging, discharging, overcurrent protection.    
**Screen:** 0.91 Inch OLED i2C with SSD1306 driver    
**Buttons:** The System button is present physically on the PCB. The cartridge button is a soldering pad on one end of the PCB. Boot and EN are exposed solder pads (You can short them and trigger the action in an emergency).    
**Sensors:** Capacitive Touch pin and Battery voltage level ADC (Needs to be calibrated)


## Sponsored by:
![Sponsor](https://github.com/AthemiS13/smart-pen/blob/main/Hardware/oshw.png)




