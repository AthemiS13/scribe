#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Preferences.h>

#define BUTTON_PIN 8
#define LONG_PRESS_DURATION 1400 //in ms
#define DEBOUNCE_TIME 50 //in ms
#define DOUBLE_PRESS_TIME 400 //in ms 

// Define I2C pins
#define OLED_SDA 5
#define OLED_SCL 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define MAX_LINE_LEN 21 //in chars

// Create the display object with the specified dimensions
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool is_screen_on = true;

//flash storage for data, allowing turning off scribe entirely witout losing last data
Preferences storage;
#define STORAGE_NAME "scribe" //do not change this if not necessary, max 15 chars
#define DATA_KEY "data" //key under which last data are stored
#define FONT_SIZE_KEY "font_size" //key under which last font_size is stored
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
      return 3;
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

void process_data_into_pages(char *buf, int l) { //l = lines per page
    int processed = 0;
    while (buf[processed] != '\0') {
      if (processed != 0) {
	  processed++;
      }
      processed += add_page(&all_pages, buf+processed, l);
    }
}

class InfoCharacteristicCallBack: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    if (to_read == -1) {
      free(data_buf);
      destroy_pages(&all_pages);
      data_buf = NULL;

      uint8_t *info = pChar -> getData();
      uint8_t version = *info;
      font_size = *(info+1);
      uint16_t data_size;
      memcpy(&data_size, info+2, 2);

      lines_per_page = get_lines_per_page((int) font_size);

      Serial.println(version);
      Serial.println(font_size);
      Serial.println(data_size);
      data_buf = (char *) malloc(sizeof(char) * (data_size+1));
      data_buf[0] = '\0';
      to_read = data_size;
      all_pages = create_pages();


    }
  }
};

class DataCharacteristicCallBack: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    if (to_read > 0) {
      String chunk = pChar->getValue();
      to_read -= chunk.length();
      strcat(data_buf, chunk.c_str());

      if (to_read == 0) {
	process_data_into_pages(data_buf, lines_per_page);
	if (all_pages.curr != NULL) {
	  display_page(all_pages.curr->data, font_size);
	  store_data(data_buf, font_size);
	}
        to_read = -1;
      }
    } 
  }
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      device_connected = true;
      advertise = false;
      display_page("Device connected!\nwaiting for data...", 1);

      pAdvertising->stop();
    };

    void onDisconnect(BLEServer* pServer) {
      device_connected = false;
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

  // Set I2C pins
  Wire.begin(OLED_SDA, OLED_SCL);

  // Initialize the display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.println(F("SSD1306 allocation failed"));
      return;
  }
  pinMode(BUTTON_PIN, INPUT_PULLDOWN);

  storage.begin(STORAGE_NAME, false); //false means read and write permissions

  if (!load_data()) { //trying to load last data from flash
    display_page("SCRIBE", 3);
  }

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
                      BLECharacteristic::PROPERTY_WRITE
  );                   

  pCharacteristic_2 = pService->createCharacteristic(
                      CHARACTERISTIC_UUID_2,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE
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
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (advertise && !device_connected) {
      display_page("Waiting for\nconnection...", 1);
      delay(500); 
      pServer->startAdvertising(); 
  }

  if (!advertise || !device_connected) {
      int btn_state = digitalRead(BUTTON_PIN);
      check_for_actions(btn_state);
  }
  delay(10);
}
