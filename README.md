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
5. You can ignore the info bellow. It only contains some old notes for me and [George](https://github.com/freddycz "George") but it is now outdated anyway because of the new PCB.

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

## Functions
**Core: **ESP32-S3FN8 Processor with integrated 8MB Flash Memory
**Communication: **Bluetooth, Wifi and Serial USB-C
**Power management:** Single cell Li-Po 3.7v Battery charging circuit with 0.6A Charging current and overcharging, discharging, overcurrent protection.
**Screen:** 0.91 Inch OLED i2C with SSD1306 driver
**Buttons:** System buton is present physically on the PCB. Cartrige button is a soldering pad on one end of the PCB. Boot and EN are exposed solder pads (You can short them and trigger the action in case of an emergency).
**Sensors:** Capacitive Touch pin and Battery voltage level ADC (Needs to be calibrated)




## Sponsored by:
![Sponsor](https://github.com/AthemiS13/smart-pen/blob/main/Hardware/oshw.png)




# SmartPen requirements:
1. BT connection **status** displayed on startup
2. ESP waits for **text** being sent through the BT serial terminal by the connected device
3. Once the text is received, ESP divides text into **Pages**, it tries to fit as much text on one page as possible.
4. ESP **does not break the provided text** (Doesn't break words at the end of each line)
5. Pages are **scrolled** using two buttons back and forward
6. **Deep Sleep mode:** Since SmartPen uses a small battery, it is crucial to save power. By pressing both buttons at the same time ESP goes into Deep Sleep mode (Viz. Espressif docs) this measure also prevents teachers from seeing what's on your display.
7. ESP is **woken up** from **Deep Sleep** using one of the buttons.
 

##### Note: Try to disengage BT communication after text is received since it draws a lot of power. The question is how to get it back running. I am not sure if i have enough space for the main power button that cuts off the power in order to forcefully restart the device. That would mean that the ESP would be running or in Deep Sleep all the time. The absence of a power button is beneficial since it saves space and transferred text does not get lost on power-off.

### Components: 
1. Two push **buttons** module: https://a.aliexpress.com/_Ey0rvsl 
2. Li-Po **Charging** board: https://a.aliexpress.com/_EGzRzOv   
 ![Charging board](https://github.com/freddycz/smart-pen/blob/main/Hardware/charging.png)
3. **ESP-32-S3 mini:** https://a.aliexpress.com/_EvDRQIV
![ESP-32-S3](https://github.com/freddycz/smart-pen/blob/main/Hardware/esp.png)
4. 0.91 Inch, 128x32 **Display** (Uses I2C communication): https://a.aliexpress.com/_Exftypj
![Display](https://github.com/freddycz/smart-pen/blob/main/Hardware/display.png)
5. Disposable vape 500 mAh Li-Po **Battery**

### Wiring:    
  **-Display:** SCL-GPIO9, SDA-GPIO8

  **-Buttons:** GPIO5, GPIO6

### Tested BT Serial app: 
BluetoothforArduino (Not ideal, takes a couple of tries to get it to connect)

# Pseudo Code

### Stage 1 (Power on, Bluetooth)

1. A chosen shows up on the screen
2. ESP enables Bluetooth and will be discoverable
3. ESP shows a status screen of the connection. This should look like this: ESP shows the status with a loading animation. For example, Searching... and the dots will be moving or something. Also in the top right corner of the display, there should be a Bluetooth icon that will light up or be crossed depending on the device's state. Use LOPAKA editor which can use u8g2. It has all the icons logos and stuff so you can just let me paste the assets into your code. Also when a device gets connected, print Connected and a thick or something again with some cool animation. If the device gets disconnected, change the icon state, but don't print anything else on the display. After notifying about a successful connection, you can print Waiting for text..., also with an animation. 

### Stage 2 (Text handling)

1. Once the device is connected successfully ESP is ready for a text to be sent 
2. Text is being sent through a BT serial terminal from an IPhone, but the BT can stay BLE, no need for regular BT, as it complicates stuff.
3. Somehow divide the screen into a text plane. It needs to exactly know where it can print text and where not. So the text needs to fit in the screens resolution, i need to fit as much information as possible. The BT status icon can sta y displayed, but text needs to avoid colliding with it.
4. Display Text on the display. DISPLAY TEXT IN A MEANINGFUL MANNER. Keep the full words and don't break text on the ends of lines. THIS IS VERY IMPORTANT since it makes the text readable. 
5. Large text handling: ESP can receive big texts through serial communication, you need to create a function to divide the text to fit as much words on one page as possible while keeping it readable (Point 4) but also to put information that wont fit first page on the next page. The number of pages can be infinite but is limited by ESP memory. 
6. Pages will be scrolled using buttons, we will deal with it in stage 3

### Stage 3 (Controls)

-A single press of each button will scroll through pages
-Press of both buttons at the same time will turn the display off (clear buffer)
-If both buttons at the same time are pressed for longer than 1,5s then enter DeepSleep mode
-A long press of Button 1 will clear text memory
-A long press of Button 2 will enter pairing mode

Note: I suggest choosing the delay values for long presses wisely, because we want to prevent accidental wipes and resets. So even 4s is fine.

### Stage 4 (DeepSleep)

-ESP can be woken up from DeepSleep by pressing one of the buttons
-ESP needs to save text even while in DeepSleep
-Deep sleep turns on automatically after 15 minutes idle

### Stage 5 (Interface)

-I will design a concept in Figma
-Both desktop app and mobile app should be available
-I understand the need for a more complex interface than just a BT terminal on an iPhone, but in case of an emergency when you don't have access to a desktop, it is critical to have a backup
-User will be able to paste custom text and to format it to his liking onto pages. Ideally, font size, type, and weight should be available, but that may be too complex. 
