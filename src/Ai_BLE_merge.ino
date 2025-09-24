#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ========== USER CONFIGURABLE VARIABLES ==========
// Pin Configuration
#define BUTTON_PIN 8
#define SYSSW_PIN 37

// BLE Mode Button Timings (in milliseconds)
#define LONG_PRESS_DURATION 1400  // Long press for BLE advertising
#define DEBOUNCE_TIME 50          // Button debounce time
#define DOUBLE_PRESS_TIME 400     // Double press detection window

// AI Mode Button Timings (in milliseconds)
#define AI_SINGLE_TAP_TIME 100       // Single tap for letter selection
#define AI_DOUBLE_TAP_WINDOW 400     // Double tap detection window
#define AI_LONG_PRESS_TIME 800       // Long press for processing/back to keyboard
#define AI_BACKSPACE_TIME 300        // Hold time for backspace

// AI Keyboard Settings
#define MOVE_INTERVAL 300            // Auto-cycle speed (ms per letter)
#define WIFI_TIMEOUT 10000           // WiFi connection timeout (ms)
#define HTTP_RETRIES 3               // Number of HTTP retry attempts

// WiFi and Gemini Configuration (FILL THESE IN!)
const char* ssid = "Wifi";
const char* password = "11111111";
const char* geminiApiKey = "";

// Static IP Configuration (helps with iPhone hotspot connectivity)
// Multiple IP ranges for different iPhone hotspot configurations
IPAddress local_IP_1(172, 20, 10, 2);    // Most common iPhone hotspot range
IPAddress gateway_1(172, 20, 10, 1);
IPAddress local_IP_2(192, 168, 43, 2);   // Alternative iPhone hotspot range
IPAddress gateway_2(192, 168, 43, 1);
IPAddress local_IP_3(10, 0, 0, 2);       // Third common range
IPAddress gateway_3(10, 0, 0, 1);
IPAddress subnet(255, 255, 255, 0);      // Subnet mask
IPAddress primaryDNS(8, 8, 8, 8);        // Google DNS
IPAddress secondaryDNS(8, 8, 4, 4);      // Google DNS secondary

// Display Settings
#define BOOT_SCREEN_DURATION 2000    // Boot screen display time (ms)
// ================================================== 

// Define I2C pins
#define OLED_SDA 5
#define OLED_SCL 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define MAX_LINE_LEN 21 //in chars

// Create the display object with the specified dimensions
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool is_screen_on = true;

//flash storage for data, allowing turning off scribe entirely without losing last data
Preferences storage;
#define STORAGE_NAME "scribe" //do not change this if not necessary, max 15 chars
#define DATA_KEY "data" //key under which last data are stored
#define FONT_SIZE_KEY "font_size" //key under which last font_size is stored
#define MODE_KEY "mode" //key under which device mode is stored
void store_data(char *data, int f_size); // Prototype needed because a function using store_data is declared earlier


#define SERVICE_UUID          "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_1 "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_2 "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"

// Initialize all pointers
BLEServer* pServer = NULL;                        
BLEAdvertising *pAdvertising = NULL;
BLECharacteristic* pCharacteristic_1 = NULL;     
BLECharacteristic* pCharacteristic_2 = NULL;    

bool device_connected = false;
bool advertise = false;

// AI Mode variables
enum DeviceMode { BLE_MODE, AI_MODE };
DeviceMode currentMode = BLE_MODE;



// AI Keyboard variables
String typedText = "";
int currentIndex = 0;
char prevLetter = 'A';
unsigned long lastMove = 0;
float foreshadowFactor = 1.3;

// AI Response paging
struct AIPage { String data; };
AIPage responsePages[10];
int numPages = 0;
int currPage = 0;

// AI Button handling
unsigned long pressStart = 0;
unsigned long lastRelease = 0;
int tapCount = 0;
bool isPressed = false;    

// Alphabet
const char* alphabet = "ABCDEFGHIJKLM"
                       "NOPQRSTUVWXYZ";
const int alphabetLen = 26;
const int charsPerRow = 13;
const int cellW = 9;
const int cellH = 10;
const int gridTop = 12;

// AI UI State
enum AIUIState { AI_TYPING, AI_PROCESSING, AI_RESPONSE, AI_ERROR };
AIUIState aiUIState = AI_TYPING;

// System button handling
bool sysBtnPressed = false;
unsigned long sysBtnPressStart = 0;
int lastBtnState = LOW;



/*
Displaying data
*/

struct page {
    struct page *next;
    struct page *prev;
    int lines;
    char data[(MAX_LINE_LEN+1)*4 + 1];
};

struct pages {
    struct page *head;
    struct page *tail;
    struct page *curr; 
};

struct pages create_pages(void) {
    struct pages all_pages;
    all_pages.curr = NULL;
    all_pages.tail = NULL;
    all_pages.head = NULL;

    return all_pages;
}


int add_page(struct pages *all_pages, char *buf, int lines) {
    struct page *new_page = (struct page *) malloc(sizeof(struct page));
    if (new_page == NULL) {
	    return 0;
    }
    new_page->lines = lines;
    int i;
    int lines_read = 0;

    for (i = 0; i < (MAX_LINE_LEN + 1) * lines; i++) { //adding 1 to max_line_len to make space for end-of-line character 
	    if (buf[i] == '\0') {
	        break;
	    }

	    if (buf[i] == '\n') {
	        lines_read++;
	        if (lines_read == lines) {
		    break;
	        }
	    }

	    new_page->data[i] = buf[i];
    }
    new_page->data[i] = '\0';

    if (all_pages->head == NULL) {
	    new_page->next = new_page;	
	    new_page->prev = new_page;

	    all_pages->curr = new_page;
	    all_pages->head = new_page;
	    all_pages->tail = new_page;
	    return i;
    }

    new_page->prev = all_pages->tail;
    new_page->next = all_pages->head;

    all_pages->tail->next = new_page;
    all_pages->head->prev = new_page;
    all_pages->tail = new_page;

    return i;
}

void next_page(struct pages *p) {
    p->curr = p->curr->next;
}

void destroy_pages(struct pages *all_pages) {
    struct page *curr = all_pages->head;
    if (curr == NULL) {
      return;
    }

    all_pages->tail->next = NULL;

    while (curr != NULL) {
	    struct page *temp = curr->next;
	    free(curr);
	    curr = temp;
    }

    all_pages->tail = NULL;
    all_pages->head= NULL;
    all_pages->curr = NULL; 
}

void display_page(char *val, int f_size) {
  if (is_screen_on) {
    display.clearDisplay();
    // Set text properties
    display.setTextSize(f_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);    
    display.println(val);
    display.display();
  } else {
    display.clearDisplay();
    display.display();
  }
}

int get_lines_per_page(int f_size) {
  switch (f_size) {
    case 1:
      return 4;
    case 2:
      return 2;
    default:
      return 1;
  }
}


struct pages all_pages = create_pages();
char *data_buf = NULL;
int to_read = -1;
uint8_t font_size = 1;
int lines_per_page = -1;

// DEBUG VARIABLES
unsigned long last_info_time = 0;
unsigned long last_data_time = 0;
int total_chunks_received = 0;
int expected_data_size = 0;

void process_data_into_pages(char *buf, int l) { //l = lines per page
    int processed = 0;
    while (buf[processed] != '\0') {
      if (processed != 0) {
	  processed++;
      }
      processed += add_page(&all_pages, buf+processed, l);
    }
}



// Load device mode from storage
DeviceMode loadMode() {
  if (storage.isKey(MODE_KEY)) {
    int mode = storage.getInt(MODE_KEY);
    return (mode == AI_MODE) ? AI_MODE : BLE_MODE;
  }
  return BLE_MODE; // Default to BLE mode
}

// Store device mode to storage
void storeMode(DeviceMode mode) {
  storage.putInt(MODE_KEY, (int)mode);
}

// AI Mode Functions
void drawKeyboard() {
  display.clearDisplay();
  String shownText = (typedText.length() > 21) ? typedText.substring(typedText.length()-21) : typedText;
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(shownText);

  for(int i=0;i<alphabetLen;i++){
    int row = i/charsPerRow;
    int col = i%charsPerRow;
    int x = col*cellW;
    int y = gridTop + row*cellH;
    display.setCursor(x+2, y+2);
    display.print(alphabet[i]);
    
    // Current selection - solid box
    if(i==currentIndex){
      display.drawRect(x,y,cellW,cellH,SSD1306_WHITE);
    }
    
  }
  
  display.display();
}

void drawAIPage() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(responsePages[currPage].data);
  display.display();
}

void drawProcessingScreen(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Processing...");
  display.display();
}

void drawErrorScreen(String msg){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Error:");
  display.setCursor(0,10);
  display.print(msg);
  display.display();
}

void drawWiFiConnectingScreen(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Connecting WiFi...");
  display.display();
}

void drawWiFiFailedScreen(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("WiFi Failed!");
  display.setCursor(0,10);
  display.print("Check config");
  display.display();
}

void drawWiFiSuccessScreen(){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("WiFi Connected!");
  display.setCursor(0,10);
  display.print("Processing...");
  display.display();
}

// Enhanced WiFi connection with multiple iPhone hotspot ranges
bool connectToWiFi() {
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi already connected");
    Serial.print("Current IP: "); Serial.println(WiFi.localIP());
    return true;
  }
  
  drawWiFiConnectingScreen();
  Serial.println("=== WiFi Connection Attempt ===");
  Serial.print("SSID: "); Serial.println(ssid);
  
  // Complete WiFi reset
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
  delay(500);
  
  // Try DHCP first
  Serial.println("Attempting DHCP connection...");
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - start < 6000) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  
  if(WiFi.status() == WL_CONNECTED) {
    Serial.println("DHCP connection successful!");
    return true;
  }
  
  // Try multiple static IP ranges for iPhone hotspots
  IPAddress configs[3][2] = {
    {local_IP_1, gateway_1},
    {local_IP_2, gateway_2}, 
    {local_IP_3, gateway_3}
  };
  
  String ranges[] = {"172.20.10.x", "192.168.43.x", "10.0.0.x"};
  
  for(int i = 0; i < 3; i++) {
    Serial.print("Trying static IP range: "); Serial.println(ranges[i]);
    
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_STA);
    delay(500);
    
    if(!WiFi.config(configs[i][0], configs[i][1], subnet, primaryDNS, secondaryDNS)) {
      Serial.println("Failed to configure static IP");
      continue;
    }
    
    WiFi.begin(ssid, password);
    start = millis();
    while(WiFi.status() != WL_CONNECTED && millis() - start < 5000) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();
    
    if(WiFi.status() == WL_CONNECTED) {
      Serial.print("Static IP connection successful with range: ");
      Serial.println(ranges[i]);
      Serial.print("IP: "); Serial.println(WiFi.localIP());
      return true;
    }
  }
  
  Serial.println("All connection attempts failed");
  return false;
}

// Gemini 2.0 Flash Call with retry & timeout
String queryAI(String input){
  // Try to connect to WiFi
  if(!connectToWiFi()) {
    drawWiFiFailedScreen();
    delay(2000);
    return "WiFi fail";
  }
  
  // Show success and network info
  Serial.println("=== WiFi Connected Successfully ===");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("Gateway: "); Serial.println(WiFi.gatewayIP());
  drawWiFiSuccessScreen();
  delay(1000);

  String apiUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent";

  String systemInstructions = 
        "You are a compact assistant for a small pen-like device with a 128x32 OLED screen called Scribe. "
        "Your responses must be:"
        "Concise, clear, and structured, suitable for a very small display.Avoid unnecessary words; prioritize key information."
        "Always answer fully and informatively, even if the user types only a single keyword,such as the name of an author, topic, or concept. Infer the intended question and provide relevant details (e.g., background, main works, context)."
        "Respond in the inputs language (English, Czech) using only the basic 26-character alphabet (A-Z), digits,punctuation, and dashes if necessary. Avoid accents, diacritics, or any foreign special characters."
        "Support inputs typed in English or Czech (without diacritics or special characters).Automatically understand the input language and provide an informative answer."
        "Format text in a way so it fits the devices screen. One line of text you respond is one page on the device. It can be maximum 84 characters. Try to fit as much informations on each line (page) as possible"
        "Prioritize accuracy and relevance over brevity if needed, but still be concise. "
        "Avoid asking clarifying questions like what do you mean by X; always assume the user wants the most likely and relevant answer."
        "Always respond in the inputs language, if your unsure, use english"
        "Avoid blank lines in the response, since the space is tight, try to fit as many useful information to the available space";

  String prompt = systemInstructions + " Input: " + input;
  String payload = "{ \"contents\": [ { \"parts\": [ { \"text\": \"" + prompt + "\" } ] } ] }";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15000);

  HTTPClient http;
  http.begin(client, apiUrl);
  http.addHeader("Content-Type","application/json");
  http.addHeader("X-goog-api-key", geminiApiKey);

  String responseText = "";
  int retries = 0;

  Serial.println("Starting API request...");
  
  while(retries < HTTP_RETRIES){
    Serial.print("Attempt "); Serial.print(retries + 1); Serial.print("/"); Serial.println(HTTP_RETRIES);
    
    // Check WiFi connection before each attempt
    if(WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected during request");
      if(!connectToWiFi()) {
        http.end();
        return "WiFi fail";
      }
    }
    
    int httpCode = http.POST(payload);
    Serial.print("HTTP Response Code: "); Serial.println(httpCode);
    
    if(httpCode == 200){
      String raw = http.getString();
      Serial.print("Response length: "); Serial.println(raw.length());
      
      if(raw.length() > 0) {
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, raw);
        if(!error){
          if(doc.containsKey("candidates") &&
             doc["candidates"][0].containsKey("content") &&
             doc["candidates"][0]["content"].containsKey("parts") &&
             doc["candidates"][0]["content"]["parts"][0].containsKey("text")) {
            responseText = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
            responseText.trim();
            Serial.println("API response received successfully");
            break;
          } else {
            Serial.println("Invalid JSON structure");
          }
        } else {
          Serial.print("JSON parse error: "); Serial.println(error.c_str());
        }
      } else {
        Serial.println("Empty response received");
      }
    } else if(httpCode > 0) {
      Serial.print("HTTP error: "); Serial.println(httpCode);
      String errorResponse = http.getString();
      Serial.print("Error details: "); Serial.println(errorResponse);
    } else {
      Serial.println("Connection failed");
    }
    
    retries++;
    if(retries < HTTP_RETRIES) {
      Serial.println("Retrying in 1 second...");
      delay(1000);
    }
  }

  if(responseText.length() == 0) {
    responseText = "HTTP fail";
    Serial.println("All HTTP attempts failed");
  }
  
  http.end();
  return responseText;
}

// Pagination by line breaks with better empty line handling
void paginateResponse(String r){
  numPages = 0;
  currPage = 0;
  r.trim(); // Remove leading/trailing whitespace

  int start = 0;
  while(start < r.length() && numPages < 10){
    int end = r.indexOf('\n', start);
    if(end == -1) end = r.length();
    String page = r.substring(start, end);
    page.trim(); // Remove whitespace from each line
    
    // Only add non-empty pages
    if(page.length() > 0){
      responsePages[numPages++].data = page;
    }
    start = end + 1;
  }

  // If no pages were created or all were empty, use the original text
  if(numPages == 0){
    String trimmed = r;
    trimmed.trim();
    if(trimmed.length() > 0) {
      responsePages[0].data = trimmed;
      numPages = 1;
    } else {
      responsePages[0].data = "No response";
      numPages = 1;
    }
  }
}

// AI Button Handling - New behavior
void handleAIButton() {
  int btn = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if(btn==HIGH && !isPressed){
    isPressed=true;
    pressStart=now;
  }

  if(btn==LOW && isPressed){
    isPressed=false;
    unsigned long dur = now - pressStart;

    if(dur >= AI_LONG_PRESS_TIME){
      // Long press behavior
      if(aiUIState == AI_TYPING && typedText.length() > 0){
        // Send for processing
        aiUIState = AI_PROCESSING;
      } else if(aiUIState == AI_RESPONSE || aiUIState == AI_ERROR){
        // Go back to keyboard
        aiUIState = AI_TYPING;
        typedText = "";
        currentIndex = 0;
      }
    } else if(dur >= AI_BACKSPACE_TIME && aiUIState == AI_TYPING){
      // Medium press for backspace (only in typing mode)
      if(typedText.length() > 0){
        typedText.remove(typedText.length()-1);
      }
    } else {
      // Short press - handle tap counting
      if(now-lastRelease < AI_DOUBLE_TAP_WINDOW) tapCount++;
      else tapCount=1;
      lastRelease=now;
    }
  }

  // Process tap actions
  unsigned long now2 = millis();
  if(tapCount==1 && (now2-lastRelease >= AI_DOUBLE_TAP_WINDOW)){
    // Single tap
    if(aiUIState == AI_TYPING){
      typedText += prevLetter;  // Add current letter
    } else if(aiUIState == AI_RESPONSE || aiUIState == AI_ERROR){
      // Toggle screen on/off for discretion
      is_screen_on = !is_screen_on;
      if(!is_screen_on) {
        display.clearDisplay();
        display.display();
      } else {
        // Redraw current screen when turning back on
        if(aiUIState == AI_RESPONSE) {
          // Will be redrawn in main loop
        } else if(aiUIState == AI_ERROR) {
          drawErrorScreen("Connection failed");
        }
      }
    }
    tapCount=0;
  } else if(tapCount==2){
    // Double tap
    if(aiUIState == AI_TYPING){
      typedText += ' ';  // Add space
    } else if(aiUIState == AI_RESPONSE && numPages > 0){
      // Cycle through pages
      currPage = (currPage+1) % numPages;
      is_screen_on = true;  // Ensure screen is on when cycling
    } else if(aiUIState == AI_ERROR){
      aiUIState = AI_TYPING;
      is_screen_on = true;
    }
    tapCount=0;
  }
}

class InfoCharacteristicCallBack: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    Serial.println("INFO_PACKET_START");
    Serial.print("to_read=");
    Serial.println(to_read);
    
    if (to_read == -1) {
      last_info_time = millis();
      
      free(data_buf);
      destroy_pages(&all_pages);
      data_buf = NULL;

      String rawValue = pChar->getValue();
      uint8_t *rawData = (uint8_t*)rawValue.c_str();
      
      uint8_t version = rawData[0];
      font_size = rawData[1];
      uint16_t data_size;
      memcpy(&data_size, rawData+2, 2);
      expected_data_size = data_size;
      
      Serial.print("version=");
      Serial.print(version);
      Serial.print(" font=");
      Serial.print(font_size);
      Serial.print(" size=");
      Serial.println(data_size);

      lines_per_page = get_lines_per_page((int) font_size);

      data_buf = (char *) malloc(sizeof(char) * (data_size+1));
      if (data_buf == NULL) {
        Serial.println("MALLOC_FAILED");
        return;
      }
      
      data_buf[0] = '\0';
      to_read = data_size;
      total_chunks_received = 0;
      all_pages = create_pages();

      Serial.print("ready_for=");
      Serial.println(to_read);
    } else {
      Serial.print("WARNING_to_read=");
      Serial.println(to_read);
    }
    Serial.println("INFO_PACKET_END");
  }
};

class DataCharacteristicCallBack: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    unsigned long current_time = millis();
    last_data_time = current_time;
    
    Serial.print("DATA_CHUNK_START to_read=");
    Serial.println(to_read);
    
    // RACE CONDITION PROTECTION
    if (to_read == -1) {
      Serial.println("RACE_CONDITION_IGNORED");
      return;
    }
    
    if (to_read > 0) {
      // Use only raw data for BLE chunk processing
      uint8_t* raw_data = pChar->getData();
      size_t raw_length = pChar->getLength();
      size_t actual_length = raw_length;

      // Limit chunk size to remaining bytes needed
      if ((int)actual_length > to_read) {
        Serial.print("CHUNK_TOO_LARGE actual=");
        Serial.print(actual_length);
        Serial.print(" needed=");
        Serial.println(to_read);
        actual_length = to_read;
      }

      total_chunks_received++;

      Serial.print("chunk#");
      Serial.print(total_chunks_received);
      Serial.print(" raw_len=");
      Serial.println(raw_length);

      // Debug: show raw bytes received
      Serial.print("raw_bytes=[");
      for (size_t i = 0; i < actual_length; i++) {
        Serial.print(raw_data[i]);
        if (i < actual_length - 1) Serial.print(",");
      }
      Serial.println("]");

      // Debug: show hex representation
      Serial.print("chunk_hex=");
      for (size_t i = 0; i < actual_length; i++) {
        if (raw_data[i] < 16) Serial.print("0");
        Serial.print(raw_data[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      // Build chunk string from raw bytes
      String chunk = "";
      for (size_t i = 0; i < actual_length; i++) {
        chunk += (char)raw_data[i];
      }

      Serial.print("final_data=\"");
      Serial.print(chunk);
      Serial.println("\"");

      int old_to_read = to_read;
      to_read -= actual_length;

      Serial.print("to_read ");
      Serial.print(old_to_read);
      Serial.print("->");
      Serial.println(to_read);

      strcat(data_buf, chunk.c_str());

      Serial.print("buf_len=");
      Serial.println(strlen(data_buf));

      // Clear characteristic value to avoid cached data
      pChar->setValue("");

      if (to_read == 0) {
        Serial.println("ALL_DATA_RECEIVED");
        Serial.print("chunks=");
        Serial.print(total_chunks_received);
        Serial.print(" expected=");
        Serial.print(expected_data_size);
        Serial.print(" actual=");
        Serial.println(strlen(data_buf));

        Serial.print("data=\"");
        Serial.print(data_buf);
        Serial.println("\"");

        Serial.println("PROCESSING_START");
        process_data_into_pages(data_buf, lines_per_page);

        if (all_pages.curr != NULL) {
          Serial.println("DISPLAY_UPDATE");
          display_page(all_pages.curr->data, font_size);
          store_data(data_buf, font_size);
          Serial.println("STORAGE_COMPLETE");
        } else {
          Serial.println("NO_PAGES_ERROR");
        }

        to_read = -1;
        Serial.println("PROCESSING_COMPLETE");
      } else if (to_read < 0) {
        Serial.print("ERROR_NEGATIVE_to_read=");
        Serial.println(to_read);
        Serial.println("RESETTING_to_read");
        to_read = -1;
      }
    } else {
      Serial.println("WARNING_INVALID_STATE");
    }
    Serial.println("DATA_CHUNK_END");
  }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      device_connected = true;
      advertise = false;
      Serial.println("\n📱 === DEVICE CONNECTED ===");
      Serial.println("📱 DEBUG: BLE connection established");
      display_page("Device connected!\nwaiting for data...", 1);

      pAdvertising->stop();
    };

    void onDisconnect(BLEServer* pServer) {
      device_connected = false;
      Serial.println("\n📱 === DEVICE DISCONNECTED ===");
      Serial.println("📱 DEBUG: BLE connection terminated");
      Serial.print("📱 DEBUG: to_read state at disconnect: ");
      Serial.println(to_read);
      pAdvertising->stop();
    }
};




/*
Button actions 
*/

void handle_single_press() {
  if (all_pages.curr != NULL) {
      if (is_screen_on) {
	is_screen_on = false;
	display_page("", 1);
      } else {
	is_screen_on = true;
	display_page(all_pages.curr->data, font_size);
      }
  }
}

void handle_double_press() {
    if (all_pages.curr != NULL) {
	next_page(&all_pages);	
	display_page(all_pages.curr->data, font_size); 
    }
}

void handle_long_press() {
    if (pServer->getConnectedCount() > 0) {
      pServer->disconnect(0);
      device_connected = false;
    }
    is_screen_on = true;
    advertise = true;
}


unsigned long pressed_time = 0; //last time when was button pressed
int press_count = 0; //used to detect double-press 
bool is_pressed = false;
bool long_press_detected = false;


//check for press, double-press and long-press 
void check_for_actions(int btn_state) {
    unsigned long now = millis();

    if (btn_state == HIGH && !is_pressed) {
        // Button just pressed 
        is_pressed = true;
        pressed_time = now;
        press_count++;
    } else if (btn_state == LOW && is_pressed) {
        // Button just released
        is_pressed = false;

        if ((now - pressed_time) >= LONG_PRESS_DURATION) {
            long_press_detected = true;
        } else {
            pressed_time = now;
        }
    } else if (btn_state == HIGH && is_pressed && (now - pressed_time) >= LONG_PRESS_DURATION) {
	long_press_detected = true;
	is_pressed = false;
    }

    // Check for events
    if (!is_pressed && (now - pressed_time > DEBOUNCE_TIME)) {
        if (long_press_detected) {
	    handle_long_press();
            press_count = 0;
            long_press_detected = false;
	    delay(1000); //gives user time to release button, without delay single-press would be immediately detected
        } else if (press_count == 1 && (now - pressed_time > DOUBLE_PRESS_TIME)) {
	    handle_single_press();
            press_count = 0;
        } else if (press_count == 2) {
	    handle_double_press();
            press_count = 0;
        }
    }
}


/*
  Flash memory management
*/
bool load_data() {
  if (!storage.isKey(DATA_KEY) || !storage.isKey(FONT_SIZE_KEY)) {
    return false;
  }

  String data_str = storage.getString(DATA_KEY);
  int data_size = data_str.length();
  data_buf = (char *) malloc(sizeof(char) * (data_size+1));
  strcpy(data_buf, data_str.c_str());
  data_buf[data_size] = '\0';

  int f_size = (int) storage.getInt(FONT_SIZE_KEY);
  lines_per_page = get_lines_per_page(f_size);


  process_data_into_pages(data_buf, lines_per_page);
  if (all_pages.curr != NULL) {
    display_page(all_pages.curr->data, font_size);
    return true;
  }
  return false;
}

void store_data(char *data, int f_size) {
  String data_str = String(data);
  storage.putString(DATA_KEY, data_str);

  storage.putInt(FONT_SIZE_KEY, f_size);
}


void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("SCRIBE DUAL-MODE FIRMWARE STARTING");

  // Set I2C pins
  Wire.begin(OLED_SDA, OLED_SCL);

  // Initialize the display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println(F("SSD1306 allocation failed"));
      return;
  }
  
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
  pinMode(SYSSW_PIN, INPUT_PULLUP);  // System button for mode switching

  storage.begin(STORAGE_NAME, false); //false means read and write permissions
  
  // Load saved mode
  currentMode = loadMode();
  Serial.print("Loaded mode: ");
  Serial.println(currentMode == AI_MODE ? "AI_MODE" : "BLE_MODE");

  if (!load_data()) { //trying to load last data from flash
    display_page("SCRIBE", 3);
  }

  Serial.println("Starting BLE initialization...");
  BLEDevice::init("SCRIBE");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic_1 = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_1,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_WRITE_NR
  );                   

  pCharacteristic_2 = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_2,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_WRITE_NR
  );  

  // add callback functions here:
  pCharacteristic_1->setCallbacks(new InfoCharacteristicCallBack());
  pCharacteristic_2->setCallbacks(new DataCharacteristicCallBack());
  
  // Start the service
  pService->start();

  // Start advertising
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  Serial.println("SCRIBE FIRMWARE READY - GPIO37 to switch modes");
}

void loop() {
  // Check system button for mode switching
  int sysBtn = digitalRead(SYSSW_PIN);
  unsigned long now = millis();
  
  // Handle system button press for mode switching
  if (sysBtn == LOW && !sysBtnPressed) {
    sysBtnPressed = true;
    sysBtnPressStart = now;
  }
  
  if (sysBtn == HIGH && sysBtnPressed) {
    sysBtnPressed = false;
    if (now - sysBtnPressStart > 500) { // Long press to switch modes
      if (currentMode == BLE_MODE) {
        currentMode = AI_MODE;
        storeMode(currentMode);  // Save mode
        aiUIState = AI_TYPING;
        typedText = "";
        currentIndex = 0;
        is_screen_on = true;  // Ensure screen is on
        Serial.println("Switched to AI Mode");
        display_page("AI Mode\nKeyboard Ready", 1);
        delay(1000);
      } else {
        currentMode = BLE_MODE;
        storeMode(currentMode);  // Save mode
        aiUIState = AI_TYPING;  // Reset AI state
        typedText = "";         // Clear AI text
        currentIndex = 0;       // Reset keyboard position
        is_screen_on = true;    // Ensure screen is on
        Serial.println("Switched to BLE Mode");
        display_page("BLE Mode", 1);
        delay(1000);
        
        // Show saved text if it exists, otherwise show default message
        if (all_pages.curr != NULL) {
          display_page(all_pages.curr->data, font_size);
        } else {
          display_page("SCRIBE", 3);
        }
      }
    }
  }
  
  // Mode-specific logic
  if (currentMode == BLE_MODE) {
    // BLE Mode logic
    if (advertise && !device_connected) {
        display_page("Waiting for\nconnection...", 1);
        delay(500); 
        pServer->startAdvertising(); 
    }

    // Always handle button actions in BLE mode
    int btn_state = digitalRead(BUTTON_PIN);
    check_for_actions(btn_state);
  } else {
    // AI Mode logic - automatic keyboard cycling with foreshadowing
    if (aiUIState == AI_TYPING) {
      static unsigned long lastCycle = 0;
      static unsigned long lastForeshadowUpdate = 0;
      
      // Update current index
      if (now - lastCycle > MOVE_INTERVAL) {
        currentIndex = (currentIndex + 1) % alphabetLen;
        lastCycle = now;
      }
      
      // Update foreshadow letter (what letter will be current soon)
      if (now - lastForeshadowUpdate >= MOVE_INTERVAL * foreshadowFactor) {
        int prevIndex = (currentIndex - 1 + alphabetLen) % alphabetLen;
        prevLetter = alphabet[prevIndex];
        lastForeshadowUpdate = now;
      }
    }
    
    // Handle AI button input
    handleAIButton();
    
    // AI Processing State Machine
    if (aiUIState == AI_PROCESSING) {
      drawProcessingScreen();
      String response = queryAI(typedText);
      if (response == "WiFi fail") {
        aiUIState = AI_ERROR;
      } else if (response == "HTTP fail") {
        aiUIState = AI_ERROR;
      } else {
        paginateResponse(response);
        aiUIState = AI_RESPONSE;
      }
      typedText = "";
      currentIndex = 0;
    }
    
    // Display current AI UI state (only if screen is on)
    if (is_screen_on) {
      if (aiUIState == AI_TYPING) {
        drawKeyboard();
      } else if (aiUIState == AI_RESPONSE) {
        drawAIPage();
      }
    } else if (aiUIState == AI_RESPONSE || aiUIState == AI_ERROR) {
      // Keep screen off but don't update display
      // Screen will be redrawn when turned back on
    }
  }
  
  delay(10);
}
