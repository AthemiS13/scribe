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
bool buttonPressed = false;
unsigned long lastPressTime = 0;
const unsigned long debounceDelay = 200; // 200ms debounce time

void IRAM_ATTR handleButtonPress() {
  unsigned long currentTime = millis();
  
  // Only process the button press if it's past the debounce time
  if (currentTime - lastPressTime > debounceDelay) {
    lastPressTime = currentTime;
    buttonPressed = true;
  }
}

// Function to wrap text to fit within the screen width
void wrapText(const char* text, int maxWidth, int y) {
  char buffer[20];  // Assuming each line won't be longer than 20 characters
  int lineStart = 0;
  int lineEnd = 0;
  
  // Split the text into lines and draw them
  while (text[lineEnd] != '\0') {
    // Check if the current line fits within the width of the display
    if (u8g2.getStrWidth(&text[lineStart]) > maxWidth) {
      // If it doesn't, move to the next line
      u8g2.drawStr(0, y, buffer);
      y += 12;  // Adjust the Y position for the next line
      lineStart = lineEnd;  // Move the start to the new line
    }
    lineEnd++;
  }
  u8g2.drawStr(0, y, &text[lineStart]);  // Draw the last line
}

void displayPage(int page) {
  u8g2.clearBuffer();
  
  // Use a larger, more readable font
  u8g2.setFont(u8g2_font_helvB08_tr);  // Larger font (8x10)

  // Wrap text: split the text into lines if it exceeds the screen width
  wrapText(pages[page][0], SCREEN_WIDTH, 12);  // First line
  wrapText(pages[page][1], SCREEN_WIDTH, 24);  // Second line

  u8g2.sendBuffer();
}

void setup() {
  // Start Serial communication for logging
  Serial.begin(115200);
  delay(1000);  // Wait for Serial to initialize

  // Initialize OLED
  Wire.begin(SDA_PIN, SCL_PIN); // Manually define SDA and SCL pins
  u8g2.begin();  // Initialize the display

  // Initialize the button
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonPress, RISING);

  // Display the first page
  displayPage(currentPage);
}

void loop() {
  if (buttonPressed) {
    // Handle button press logic (single click: go to the next page)
    currentPage = (currentPage + 1) % totalPages;
    displayPage(currentPage);
    
    Serial.print("Switched to page: ");
    Serial.println(currentPage + 1);

    // Reset the button state
    buttonPressed = false;
  }

  delay(50);  // Short delay to keep the loop responsive
}
