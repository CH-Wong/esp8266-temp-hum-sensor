// Import required libraries
#include <Arduino.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#define DHTTYPE    DHT11     // DHT 11
#define DHTPIN 2     // Digital pin connected to the DHT sensor

DHT dht(DHTPIN, DHTTYPE);

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Replace with your network credentials
const char* ssid = "Halloballo";
const char* password = "Ballohallo";


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Initialize variables for looping data into

const int N = 744; // 1 per hour for 31 days
const int delayTime = 3600000; // [ms]

int t;
int tempArray[N];
char tempStr[3*N];

int h;
int humArray[N];
char humStr[3*N];

void startWifi() {
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.println("...");
  }

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html");
  });
  
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", tempStr);
  });
  
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", humStr);
  });
  
  // Start server
  server.begin();
}

void addToArray(int targetArray[N], int inputData){
  for (int i = 0; i < N-1; i++) {
    targetArray[i] = targetArray[i+1];
  }
  targetArray[N-1] = inputData;
}

void arrayToStr(char targetArray[3*N], int inputArray[N]) {
  strcpy(targetArray, "");
  
  for (int i = 0; i < N; i++) {   
//    if (inputArray[i] < 10) {
//      char tempChar[2]; 
//      strcat(targetArray, "0");
//      sprintf(tempChar, "%i", inputArray[i]);
//      strcat(targetArray, tempChar);
//    }

    if (inputArray[i] < 100) {
      char tempChar[3]; 
      sprintf(tempChar, "%i", inputArray[i]);
      strcat(targetArray, tempChar);
    }
    
    else {
      strcat(targetArray, "99");
    }

    if (i < N - 1) {
      strcat(targetArray, ",");
    }
  }
}

void updateData() {
  t = dht.readTemperature();
  addToArray(tempArray, t);
  arrayToStr(tempStr, tempArray);
  Serial.print("Temperature: ");
  Serial.println(tempStr);
  
  h = dht.readHumidity();
  addToArray(humArray, h);
  arrayToStr(humStr, humArray);
  Serial.print("Humidity: ");
  Serial.println(humStr);
}

void startScreen() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
  Serial.println(F("SSD1306 allocation failed"));
  for(;;);
  }
  delay(2000);
}

void updateScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 20);
  
  char tempPrint[20];
  sprintf(tempPrint, "Temperature: %d C\n", t);
  display.println(tempPrint);
  Serial.println(tempPrint);

  char humPrint[16];
  sprintf(humPrint, "Humidity: %d %%", h);
  display.println(humPrint);

  Serial.println(humPrint);
  
  display.display(); 
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  // Initialize SPIFFS
  if(!SPIFFS.begin()){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Initialize measurement device and get first data point
  dht.begin(); 
  updateData();

  startScreen();
  updateScreen();
  
  // Start wifi instance
  startWifi();
}
 
void loop(){
  updateData();
  updateScreen();
  
  delay(delayTime);
}
