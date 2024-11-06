#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <NimBLEDevice.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const int button1Pin = 5;
const int button2Pin = 6;
const unsigned long pairingHoldTime = 3000; // 3 seconds for pairing mode activation
const unsigned long toggleWindow = 600; // 200 ms window to detect both button presses
unsigned long button1HoldStart = 0; // Timer for button 1 hold
const unsigned long resetHoldTime = 3000; // 3 seconds for reset

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;
String receivedText = "";
int currentPage = 0;
bool displayOn = true; // Track display status
const int maxPages = 10;
String pages[maxPages];
unsigned long statusDisplayTime = 0;
const unsigned long statusDisplayDuration = 2000;
unsigned long button2HoldStart = 0; // Track button2 hold start time
unsigned long lastButtonPressTime = 0; // Track time of last button press

#define SERVICE_UUID "12345678-1234-1234-1234-123456789012"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-210987654321"

void divideIntoPages(String text);
void showStatus(const char* status);
void updateDisplay();
void toggleDisplay();

class MyServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
    showStatus("Connected");
  }

  void onDisconnect(NimBLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
    showStatus("Waiting...");
  }
};

class MyCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    if (value.length() > 0) {
      receivedText = String(value.c_str());
      Serial.print("Received: ");
      Serial.println(receivedText);
      divideIntoPages(receivedText);
      currentPage = 0;
      if (displayOn) updateDisplay();
    }
  }
};

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.setDisplayRotation(U8G2_R2);
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);

  NimBLEDevice::init("ESP32_SmartPen");
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  pServer->getAdvertising()->start();
  Serial.println("Waiting for a connection...");
}



void loop() {
  bool button1Pressed = digitalRead(button1Pin) == LOW;
  bool button2Pressed = digitalRead(button2Pin) == LOW;

  // Check if both buttons are pressed together for display toggle
  if (button1Pressed && button2Pressed) {
    toggleDisplay();
    delay(300); // Debounce delay for combined button press
  }

  // Check for long press on button 2 for pairing mode
  if (button2Pressed) {
    if (button2HoldStart == 0) button2HoldStart = millis(); // Start timer
    else if (millis() - button2HoldStart >= pairingHoldTime) {
      pServer->startAdvertising(); // Re-enter pairing mode
      Serial.println("Entered pairing mode");
      showStatus("Pairing mode");
      button2HoldStart = 0; // Reset hold timer after activating pairing
    }
  } else {
    button2HoldStart = 0; // Reset hold timer if button is released
  }

  // Check for long press on button 1 for reset
  if (button1Pressed) {
    if (button1HoldStart == 0) button1HoldStart = millis(); // Start timer
    else if (millis() - button1HoldStart >= resetHoldTime) {
      Serial.println("Resetting ESP32...");
      esp_restart(); // Trigger a reset of the ESP32
    }
  } else {
    button1HoldStart = 0; // Reset hold timer if button is released
  }

  // Pagination controls, only active if display is on
  if (displayOn) {
    if (button1Pressed && !button2Pressed) {
      if (currentPage > 0) {
        currentPage--;
        updateDisplay();
        delay(300); // Debounce delay for pagination
      }
    }
    if (!button1Pressed && button2Pressed) {
      if (currentPage < maxPages - 1 && !pages[currentPage + 1].isEmpty()) {
        currentPage++;
        updateDisplay();
        delay(300); // Debounce delay for pagination
      }
    }
  }

  // Automatically switch back to text display after showing connection status
  if (millis() - statusDisplayTime > statusDisplayDuration && statusDisplayTime > 0) {
    if (displayOn) updateDisplay();
    statusDisplayTime = 0;
  }
}


void divideIntoPages(String text) {
  int lineWidth = SCREEN_WIDTH - 10;
  int lineHeight = 12;
  int linesPerPage = SCREEN_HEIGHT / lineHeight;

  String line, page;
  int lineCount = 0, pageCount = 0;
  for (int i = 0; i < text.length(); i++) {
    line += text[i];
    if (u8g2.getStrWidth(line.c_str()) >= lineWidth || text[i] == '\n') {
      page += line + "\n";
      line = "";
      lineCount++;
      if (lineCount >= linesPerPage) {
        pages[pageCount++] = page;
        page = "";
        lineCount = 0;
        if (pageCount >= maxPages) break;
      }
    }
  }
  if (page.length() > 0 && pageCount < maxPages) pages[pageCount] = page;
}

void showStatus(const char* status) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(0, 12, status);
  u8g2.sendBuffer();
  statusDisplayTime = millis();
}

void updateDisplay() {
  if (!displayOn) return; // Only update if display is on

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  int y = 12;

  if (!pages[currentPage].isEmpty()) {
    int lineStart = 0;
    for (int i = 0; i < pages[currentPage].length(); i++) {
      if (pages[currentPage][i] == '\n' || i == pages[currentPage].length() - 1) {
        u8g2.drawStr(0, y, pages[currentPage].substring(lineStart, i).c_str());
        y += 12;
        lineStart = i + 1;
      }
    }
  }
  u8g2.sendBuffer();
}

void toggleDisplay() {
  displayOn = !displayOn;
  if (!displayOn) {
    u8g2.clearDisplay(); // Turn off the display
  } else {
    updateDisplay(); // Turn on and update with saved content
  }
}
