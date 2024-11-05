//This version was working fine, but the text management is poor and the code is very unstable. Also bad bluetooth performance

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <NimBLEDevice.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET    -1  // No reset pin
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Definice pinů tlačítek
const int button1Pin = 5;  // Previous page button
const int button2Pin = 6;  // Next page button

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
String receivedText = "";
int currentPage = 0;  // Current page index
const int maxLinesPerPage = 2;  // Number of lines that fit on the display
String pages[10];  // Store pages (increase size if needed)

// BLE UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321"

// Function declarations
void divideIntoPages(String text);
void updateDisplay(String text);

// Callbacks for BLE Server connection/disconnection events
class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
    updateDisplay("Connected");
  }

  void onDisconnect(NimBLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
    updateDisplay("Waiting...");
  }
};

// Callbacks for BLE characteristic write events
class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      receivedText = String(value.c_str());
      Serial.print("Received: ");
      Serial.println(receivedText);
      
      // Divide the text into pages
      divideIntoPages(receivedText);
      currentPage = 0;  // Reset to first page
      updateDisplay(pages[currentPage]);  // Display the first page
    }
  }
};

void setup() {
  Serial.begin(115200);
  
  // Initialize OLED display
  u8g2.begin();
  updateDisplay("Initializing...");

  // Set button pins
  pinMode(button1Pin, INPUT_PULLUP); // Using pull-up resistor
  pinMode(button2Pin, INPUT_PULLUP);

  // Initialize BLE
  NimBLEDevice::init("ESP32_SmartPen");
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE service
  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // Create BLE characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      NIMBLE_PROPERTY::READ |
                      NIMBLE_PROPERTY::WRITE
                    );
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a connection...");
}

void loop() {
  // Check button states for pagination
  if (digitalRead(button1Pin) == LOW) {  // Previous page
    if (currentPage > 0) {
      currentPage--;
      updateDisplay(pages[currentPage]);
      delay(300); // Debounce delay
    }
  }
  if (digitalRead(button2Pin) == LOW) {  // Next page
    if (currentPage < (sizeof(pages) / sizeof(pages[0]) - 1) && pages[currentPage + 1] != "") {
      currentPage++;
      updateDisplay(pages[currentPage]);
      delay(300); // Debounce delay
    }
  }
}

// Function to divide the received text into pages without breaking words
void divideIntoPages(String text) {
  pages[0] = "";  // Clear previous pages
  int lineCount = 0;
  String line = "";
  
  // Split the text into words
  int lastSpaceIndex = 0;  // To keep track of last space index
  for (int i = 0; i < text.length(); i++) {
    line += text[i];
    
    // Check if the current line fits the screen width
    if (line.length() > SCREEN_WIDTH / 6) {  // Approx. number of characters that fit on one line
      if (lastSpaceIndex > 0) {
        // If there's a space, move to the next line
        pages[lineCount] += line.substring(0, lastSpaceIndex) + "\n";
        line = line.substring(lastSpaceIndex + 1);  // Keep the rest of the line
      } else {
        // If no space found, just add the line as is
        pages[lineCount] += line + "\n";  // Add current line
        line = "";  // Reset line
      }
      lineCount++;
      lastSpaceIndex = 0;  // Reset last space index
    } else if (text[i] == ' ') {
      lastSpaceIndex = line.length();  // Update last space index
    }

    // If maximum pages reached, break
    if (lineCount >= sizeof(pages) / sizeof(pages[0])) {
      break;
    }
  }

  // Add remaining line if any
  if (line.length() > 0 && lineCount < sizeof(pages) / sizeof(pages[0])) {
    pages[lineCount] += line;  // Add the last line to the last page
  }
}

// Function to update the display with the given text
void updateDisplay(String text) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  
  // Draw each line of the text
  int y = 12;  // Starting Y position
  int lineHeight = 12;  // Approximate height of each line
  int lineCount = 0;
  
  // Split text into lines to fit on the display
  String line;
  for (int i = 0; i < text.length(); i++) {
    line += text[i];
    
    // Check if the current line fits the screen width
    if (line.length() > SCREEN_WIDTH / 6) {  // Approx. number of characters that fit on one line
      line.remove(line.length() - 1);  // Remove last character that doesn't fit
      u8g2.drawStr(0, y, line.c_str());  // Draw the current line
      y += lineHeight;  // Move down for the next line
      line = "";  // Reset line
      lineCount++;
    }

    // If maximum lines reached, break
    if (lineCount >= maxLinesPerPage) {
      break;
    }
  }

  // Draw remaining line if any
  if (line.length() > 0) {
    u8g2.drawStr(0, y, line.c_str());
  }

  u8g2.sendBuffer();  // Update the display
}
