#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// OLED Setup with fixed SCL (pin 4) and SDA (pin 5)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

// Define the I2C pins
#define SDA_PIN 5
#define SCL_PIN 4
#define BUTTON_PIN 8  // GPIO pin for the button

// Initialize U8g2 with I2C (using the defined SDA and SCL pins)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, SDA_PIN, SCL_PIN);

// Text to display (split across 5 pages)
const char* pages[][2] = {
  {"Welcome to the", "OLED Display Demo!"},
  {"This is a simple", "project for testing."},
  {"Press the button", "to change pages."},
  {"This is page four.", "Enjoy the demo!"},
  {"Last page. Thanks", "for trying it out!"}
};
const int totalPages = sizeof(pages) / sizeof(pages[0]);

int currentPage = 0;
bool isScreenOn = true;

// Timing thresholds (in milliseconds)
#define DEBOUNCE_TIME 50
#define LONG_PRESS_TIME 1000
#define DOUBLE_PRESS_TIME 400

// Button state tracking
unsigned long lastPressTime = 0;
unsigned long pressStartTime = 0;
bool buttonPressed = false;
bool longPressDetected = false;
int pressCount = 0;

void displayPage(int page) {
  if (isScreenOn) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);  
    u8g2.drawStr(0, 12, pages[page][0]);
    u8g2.drawStr(0, 24, pages[page][1]);
    u8g2.sendBuffer();
  } else {
    u8g2.clearBuffer();
    u8g2.sendBuffer();  // Ensure screen is off
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  u8g2.begin();

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);

  displayPage(currentPage);
}

void loop() {
  static unsigned long lastUpdateTime = 0;
  unsigned long currentTime = millis();
  
  int buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == HIGH && !buttonPressed) {
    // Button just pressed
    buttonPressed = true;
    pressStartTime = currentTime;
    pressCount++;
  } else if (buttonState == LOW && buttonPressed) {
    // Button just released
    buttonPressed = false;

    if ((currentTime - pressStartTime) >= LONG_PRESS_TIME) {
      longPressDetected = true;
    } else {
      lastPressTime = currentTime;
    }
  }

  // Check for events
  if (!buttonPressed && (currentTime - lastPressTime > DEBOUNCE_TIME)) {
    if (longPressDetected) {
      // Long press: Move one page backward
      currentPage = (currentPage - 1 + totalPages) % totalPages;
      displayPage(currentPage);
      Serial.println("Long Press: Moved one page backward.");
      pressCount = 0;
      longPressDetected = false;
    } else if (pressCount == 1 && (currentTime - lastPressTime > DOUBLE_PRESS_TIME)) {
      // Single press: Toggle screen on/off
      isScreenOn = !isScreenOn;
      displayPage(currentPage);
      Serial.println(isScreenOn ? "Single Press: Screen turned on." : "Single Press: Screen turned off.");
      pressCount = 0;
    } else if (pressCount == 2) {
      // Double press: Move one page forward
      currentPage = (currentPage + 1) % totalPages;
      displayPage(currentPage);
      Serial.println("Double Press: Moved one page forward.");
      pressCount = 0;
    }
  }

  delay(10); // Small delay for stability
}
