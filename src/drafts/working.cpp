#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <NimBLEDevice.h>


// OLED Setup with fixed SCL (pin 4) and SDA (pin 5)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

// Define the I2C pins
#define SDA_PIN 5
#define SCL_PIN 4

// Initialize U8g2 with I2C (using the defined SDA and SCL pins)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, SDA_PIN, SCL_PIN);

// Bluetooth Setup
NimBLECharacteristic* pCharacteristic;  // Characteristic to receive data
bool deviceConnected = false;
String receivedText = "";  // To store the received text

// Callback class to handle connection events
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, "Device connected");
    u8g2.sendBuffer();
  }

  void onDisconnect(NimBLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, "Device disconnected");
    u8g2.sendBuffer();
  }
};

// Callback class to handle write events on the characteristic
class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      receivedText = String(value.c_str());  // Store the received text
      Serial.print("Received text: ");
      Serial.println(receivedText.c_str());  // Print received text to Serial Monitor

      // Display the received text on the OLED
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.drawStr(0, 12, "Received Text:");
      u8g2.drawStr(0, 24, receivedText.c_str());  // Print text onto the OLED
      u8g2.sendBuffer();
    }
  }
};

void setup() {
  // Start Serial communication for logging
  Serial.begin(115200);
  delay(1000);  // Wait for Serial to initialize

  // Initialize OLED
  Wire.begin(SDA_PIN, SCL_PIN); // Manually define SDA and SCL pins
  u8g2.begin();  // Initialize the display

  // Display a message on the OLED for debugging
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, "Initializing OLED...");
  u8g2.sendBuffer();

  // Wait for a few seconds to check if the OLED works
  delay(3000);

  // Initialize BLE (Bluetooth Low Energy)
  Serial.println("Initializing ESP32 as Bluetooth device...");
  NimBLEDevice::init("ESP32_Bluetooth_Serial");
  Serial.println("BLE Initialized!");

  // Create BLE server and set callbacks
  NimBLEServer* pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create a service
  NimBLEService* pService = pServer->createService("12345678-1234-1234-1234-123456789012");

  // Create a characteristic with READ and WRITE properties
  pCharacteristic = pService->createCharacteristic(
                        "87654321-4321-4321-4321-210987654321",
                        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  
  // Set callback for the characteristic
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  
  // Start the service
  pService->start();
  
  // Start advertising (make ESP32 visible to other devices)
  NimBLEDevice::getAdvertising()->start();
  Serial.println("Advertising started. ESP32 is now visible.");
  
  // Display connection status on OLED
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "Waiting for device...");
  u8g2.sendBuffer();
}

void loop() {
  // Keep checking if the device is connected and print a message every 2 seconds
  if (deviceConnected) {
    Serial.println("Device connected and ready to receive text...");
    delay(2000);  // Delay for 2 seconds
  } else {
    Serial.println("Waiting for connection...");
    delay(2000);  // Delay for 2 seconds
  }
}
