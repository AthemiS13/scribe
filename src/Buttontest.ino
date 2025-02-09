#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// OLED Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1

#define SDA_PIN 5
#define SCL_PIN 4

// Initialize U8g2 with I2C
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, SDA_PIN, SCL_PIN);

// Pin Definitions
#define BUTTON_PIN 8

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

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialize OLED
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();

    // Display initial message
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, "Initializing...");
    u8g2.sendBuffer();
    delay(2000);

    // Setup input pin
    pinMode(BUTTON_PIN, INPUT_PULLDOWN);

    // Display ready message
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "Ready");
    u8g2.sendBuffer();
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
            displayEvent("Long Press");
            pressCount = 0;
            longPressDetected = false;
        } else if (pressCount == 1 && (currentTime - lastPressTime > DOUBLE_PRESS_TIME)) {
            displayEvent("Single Press");
            pressCount = 0;
        } else if (pressCount == 2) {
            displayEvent("Double Press");
            pressCount = 0;
        }
    }

    delay(10); // Small delay for stability
}

void displayEvent(const char* event) {
    Serial.println(event);

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 12, event);
    u8g2.sendBuffer();
}
