#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_SDA 5
#define OLED_SCL 4
#define BUTTON_PIN 8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// WiFi and Gemini config
const char* ssid = "";
const char* password = "";
const char* geminiApiKey = "";

// --- Keyboard ---
String typedText = "";
int currentIndex = 0;
char prevLetter = 'A';
unsigned long lastMove = 0;
float foreshadowFactor = 0.4;
const unsigned long moveInterval = 350;

// Response paging
struct Page { String data; };
Page responsePages[10];
int numPages = 0;
int currPage = 0;

// Button handling
unsigned long pressStart = 0;
unsigned long lastRelease = 0;
int tapCount = 0;
bool isPressed = false;
const unsigned long doubleTapWin = 400;
const unsigned long longPressTime = 1000;  
const unsigned long confirmTime   = 3000;    

// Alphabet
const char* alphabet = "ABCDEFGHIJKLM"
                       "NOPQRSTUVWXYZ";
const int alphabetLen = 26;
const int charsPerRow = 13;
const int cellW = 9;
const int cellH = 10;
const int gridTop = 12;

// --- UI State ---
enum UIState { TYPING, PROCESSING, AI_RESPONSE, ERROR };
UIState uiState = TYPING;

// --- Functions ---
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

// --- Gemini 2.0 Flash Call with retry & timeout ---
String queryAI(String input){
  if(WiFi.status() != WL_CONNECTED){
    WiFi.reconnect();
    unsigned long start = millis();
    while(WiFi.status() != WL_CONNECTED && millis() - start < 5000) delay(200);
    if(WiFi.status() != WL_CONNECTED) return "WiFi fail";
  }

  String apiUrl = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent";

  String systemInstructions = 
        "You are a compact assistant for a small pen-like device with a 128x32 OLED screen called Scribe. "
        "Your responses must be:"
        "Concise, clear, and structured, suitable for a very small display.Avoid unnecessary words; prioritize key information."
        "Always answer fully and informatively, even if the user types only a single keyword,such as the name of an author, topic, or concept. Infer the intended question and provide relevant details (e.g., background, main works, context)."
        "Respond in English text using only the basic 26-character alphabet (A-Z), digits,punctuation, and dashes if necessary. Avoid accents, diacritics, or any foreign special characters."
        "Support inputs typed in English or Czech (without diacritics or special characters).Automatically understand the input language and provide an informative answer."
        "Prioritize accuracy and relevance over brevity if needed, but still be concise. "
        "Avoid asking clarifying questions like what do you mean by X; always assume the user wants the most likely and relevant answer.";


  String prompt = systemInstructions + " Input: " + input;
  String payload = "{ \"contents\": [ { \"parts\": [ { \"text\": \"" + prompt + "\" } ] } ] }";

  WiFiClientSecure client;
  client.setInsecure(); // bypass SSL for reliable testing
  client.setTimeout(15000); // 15s timeout

  HTTPClient http;
  http.begin(client, apiUrl);
  http.addHeader("Content-Type","application/json");
  http.addHeader("X-goog-api-key", geminiApiKey);

  String responseText = "";
  int retries = 0;

  while(retries < 3){
    int httpCode = http.POST(payload);
    if(httpCode == 200){
      String raw = http.getString();
      Serial.println("Raw Gemini response:");
      Serial.println(raw);

      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, raw);
      if(!error){
        if(doc.containsKey("candidates") &&
           doc["candidates"][0].containsKey("content") &&
           doc["candidates"][0]["content"].containsKey("parts") &&
           doc["candidates"][0]["content"]["parts"][0].containsKey("text")) {
          responseText = doc["candidates"][0]["content"]["parts"][0]["text"].as<String>();
          responseText.trim();
        }
      }
      break; // success
    } else {
      Serial.print("HTTP error ");
      Serial.println(httpCode);
      retries++;
      delay(500);
    }
  }

  if(retries == 3) responseText = "HTTP fail";
  http.end();
  return responseText;
}

// --- Pagination by line breaks ---
void paginateResponse(String r){
  numPages = 0;
  currPage = 0;

  int start = 0;
  while(start < r.length() && numPages < 10){
    int end = r.indexOf('\n', start);
    if(end == -1) end = r.length();
    String page = r.substring(start, end);
    if(page.length() == 0) page = " "; // avoid empty page
    responsePages[numPages++].data = page;
    start = end + 1;
  }

  if(numPages == 0){
    responsePages[0].data = r;
    numPages = 1;
  }
}

// --- Button Handling ---
void handleButton() {
  int btn = digitalRead(BUTTON_PIN);
  unsigned long now = millis();

  if(btn==HIGH && !isPressed){
    isPressed=true;
    pressStart=now;
  }

  if(btn==LOW && isPressed){
    isPressed=false;
    unsigned long dur = now - pressStart;

    if(dur>=confirmTime){
      if(uiState == TYPING && typedText.length()>0){
        uiState = PROCESSING;
      }
    } else if(dur >= longPressTime){
      if(uiState == TYPING && typedText.length()>0){
        typedText.remove(typedText.length()-1);
        Serial.println("Deleted last character.");
      }
    } else {
      if(now-lastRelease < doubleTapWin) tapCount++;
      else tapCount=1;
      lastRelease=now;
    }
  }

  unsigned long now2 = millis();
  if(tapCount==1 && (now2-lastRelease>=doubleTapWin)){
    if(uiState==TYPING){
      typedText += prevLetter;
      Serial.print("Selected letter: ");
      Serial.println(prevLetter);
    } else if(uiState==AI_RESPONSE && numPages>0){
      currPage = (currPage+1) % numPages;
      Serial.print("AI page: ");
      Serial.println(currPage);
    } else if(uiState==ERROR){
      uiState = TYPING;
    }
    tapCount=0;
  } else if(tapCount==2){
    if(uiState==AI_RESPONSE){
      uiState = TYPING;
      Serial.println("Back to typing mode");
    } else if(uiState==TYPING){
      typedText += ' ';
      Serial.println("Added space");
    } else if(uiState==ERROR){
      uiState = TYPING;
    }
    tapCount=0;
  }
}

// --- Setup & Loop ---
void setup(){
  Serial.begin(115200);
  Wire.begin(OLED_SDA, OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)) for(;;);

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("SCRIBE Keyboard");
  display.display();
  delay(1500);

  WiFi.begin(ssid,password);
  Serial.print("Connecting WiFi");
  while(WiFi.status()!=WL_CONNECTED){delay(500);Serial.print(".");}
  Serial.println("\nWiFi connected");
}

void loop(){
  unsigned long now=millis();

  if(uiState==TYPING){
    if(now-lastMove >= moveInterval){
      currentIndex=(currentIndex+1)%alphabetLen;
      lastMove=now;
    }

    static unsigned long lastForeshadowUpdate=0;
    if(now-lastForeshadowUpdate >= moveInterval*foreshadowFactor){
      int prevIndex=(currentIndex-1+alphabetLen)%alphabetLen;
      prevLetter=alphabet[prevIndex];
      lastForeshadowUpdate=now;
    }

    handleButton();
    drawKeyboard();

  } else if(uiState==PROCESSING){
    drawProcessingScreen();
    String res = queryAI(typedText);
    if(res == "WiFi fail" || res == "HTTP fail"){
      uiState = ERROR;
      drawErrorScreen(res);
    } else {
      paginateResponse(res);
      uiState = AI_RESPONSE;
    }
  } else if(uiState==AI_RESPONSE){
    handleButton();
    drawAIPage();
  } else if(uiState==ERROR){
    handleButton();
  }

  delay(10);
}
