# SmartPen requirements:
1. BT connection status displayed on startup
2. ESP waits for text being sent through BT serial terminal by the connected device
3. Once text is received, ESP divides text into Pages, it tries to fit as much text on one page as possible.
4. ESP does not break the provided text (Doesn't break words on the end of each line)
5. Pages are scrolled using two buttons back and forward
6. Deep Sleep mode: Since SmartPen uses small battery, it is crucial to save power. By pressing both buttons at the same time
   ESP goes into Deep Sleep mode (Viz. Espressif docs) this measure also prevents teachers from seeing what's on your display.
7. ESP is woken up from Deep Sleep using one of the buttons.
 

##### Note: Try to disengage BT communication after text is received since it draws a lot of power. The question is how to get it back running. I am not sure if i have enough space for main power button that cuts off the power in order to forcefully restart the device. That would mean that the ESP would be running or in Deep Sleep all the time. Absence of power button is beneficial since it saves space and transfered text does not get lost on poweroff.

### Components: 
1. Two push buttons module: https://a.aliexpress.com/_Ey0rvsl 
2. Li-Po Charging board: https://a.aliexpress.com/_EGzRzOv
3. ESP-32-S3 mini: https://a.aliexpress.com/_EvDRQIV
4. 0.91 Inch, 128x32 Display (Uses I2C communication): https://a.aliexpress.com/_Exftypj
5. Disposable vape 500 mAh Li-Po battery

### Wiring:    
  -Display: SCL-GPIO9, SDA-GPIO8
  -Buttons: GPIO5, GPIO6

### Tested BT Serial app: 
BluetoothforArduino (Not ideal, takes a couple tries to get it to connect)

