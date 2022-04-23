#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <DHTesp.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h> // Provide the token generation process info.

#include <config.h>

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Declare DHT sensor type and properties
#define DHTTYPE DHT11     // DHT 11
#define DHTPIN 2     // GPIO pin out, not Digital pin number

// GLOBAL VARIABLE DECLARATION
DHTesp dht; // Setup classname for dht sensor

// For Network Time Protocol (NTP)
WiFiUDP UDP; // setup class name for WidiUDP connection to NTP server
IPAddress timeServerIP;          // time.nist.gov NTP server address
const char* NTPServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

// Init Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

char timeAddress[40];
char tempAddress[40];
char humAddress[40];


// NTP variable declaration
#define ONE_HOUR 3600000UL
const unsigned long intervalNTP = ONE_HOUR; // Update the time every hour
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();

unsigned long prevMeasurement = 0;
uint32_t timeUNIX = 0;                      // The most recent timestamp received from the time server


void initWiFi() {
  // WIFI INIT
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);             // Connect to the network
  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID); Serial.println(" ...");

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(1000);
    Serial.print(++i); Serial.print(' ');
  }

  Serial.println('\n');
  Serial.println("Connection established!");
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());         // Send the IP address of the ESP8266 to the computer
}


void initFirebase() {
    Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

    /* Assign the api key (required) */
    config.api_key = API_KEY;

    /* Assign the RTDB URL (required) */
    config.database_url = DATABASE_URL;

    /* Assign the sevice account JSON file and the file storage type (required) */
    config.service_account.json.path = SERVICE_ACCOUNT_FILENAME;   // change this for your json file
    config.service_account.json.storage_type = mem_storage_type_flash; // or  mem_storage_type_sd

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    Firebase.reconnectWiFi(true);

    fbdo.setResponseSize(4096);

    /** Assign the maximum retry of token generation */
    config.max_token_generation_retry = 5;

    Firebase.begin(&config, &auth);
  
    delay(100);
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}


uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  
  return UNIXTime;
}


inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}


void initNTP() {
  // INIT NTP
  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { // Get the IP address of the NTP server
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  Serial.println("Starting UDP");
  UDP.begin(123);                          // Start listening for UDP messages on port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();

  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
  
  Serial.println("\r\nSending NTP request ...");
  sendNTPpacket(timeServerIP);  
}

void resetDisplay() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);  

    display.println(STATION_NAME);
    display.println(WiFi.localIP());
    display.println("-----------------\n");
}


void setup() {
    // BOARD INIT
    Serial.begin(115200);         // Start the Serial communication to send messages to the computer

    String stationName = STATION_NAME;
    sprintf(timeAddress, "/%s/time", stationName.c_str());
    sprintf(tempAddress, "/%s/temperature", stationName.c_str());
    sprintf(humAddress, "/%s/humidity", stationName.c_str());

    // Init DHT sensor
    dht.setup(DHTPIN, DHTesp::DHT11); // Connect DHT sensor to GPIO 17

    // Init display
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
      Serial.println(F("SSD1306 allocation failed"));
    }
    else {
      Serial.println("Succesfully intialised SSD1306 Display.");
      resetDisplay();
    }

    // Initialization fucntions
    initWiFi();
    initNTP();
    initFirebase();

}


void loop(void) {
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // Request the time from the time server every hour
    prevNTP = currentMillis;
    sendNTPpacket(timeServerIP);
  }

  uint32_t time = getTime();                   // Check if the time server has responded, if so, get the UNIX time
  if (time) {
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = millis();
  } 
  else if ((millis() - lastNTPResponse) > 24UL * ONE_HOUR) {
    Serial.println("More than 24 hours since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  if (timeUNIX != 0) {

    if (Firebase.ready() && currentMillis - prevMeasurement > MEASUREMENT_INTERVAL) { // 750 ms after requesting the temperature
      prevMeasurement = currentMillis;
      uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
      // The actual time is the last NTP time plus the time that has elapsed since the last NTP response

      // Get the measurements from the sensor!
      float temp = dht.getTemperature();
      float hum = dht.getHumidity();

      // If measurement error, output 0.0 instead
      if (isnan(temp) ||  isnan(hum)) {
          temp = 0.0;
          hum = 0.0;
      }
      else {
          // Otherwise, correctly format the measurement to 2 digits
          temp = round(temp * 100.0) / 100.0; // round temperature to 2 digits
          hum = round(hum * 100.0) / 100.0; // round temperature to 2 digits
      }

      // Print the measurement in the Serial Monitor for debugging
      char buffer[50];
      sprintf(buffer, "Measurement: %d,\t%f,\t%f", actualTime, temp, hum);
      Serial.println(buffer);

      // Keep trying to push until the database update returns true
      while (!Firebase.RTDB.pushInt(&fbdo, timeAddress, actualTime)) { 
        Serial.println(fbdo.errorReason().c_str());
      };
      Serial.println("ok");

      while (!Firebase.RTDB.pushFloat(&fbdo, tempAddress, temp)) { 
        Serial.println(fbdo.errorReason().c_str());
      };
      Serial.println("ok");

      while (!Firebase.RTDB.pushFloat(&fbdo, humAddress, hum)) { 
        Serial.println(fbdo.errorReason().c_str());
      };
      Serial.println("ok");

      // Update the display to match the pushed measurement
      resetDisplay();

      static char strBuffer[30];
      char tempPrint[30];
      dtostrf(temp, 2, 2, strBuffer);
      sprintf(tempPrint, "Temperature: %s'C\n", strBuffer);
      display.println(tempPrint);
      
      char humPrint[30];
      dtostrf(hum, 2, 1, strBuffer);
      sprintf(humPrint, "Humidity: %s %%", strBuffer);
      display.println(humPrint);
      
      display.display(); 
    }
  } 

  // If we didn't receive an NTP response yet, send another request
  else {                                    
    sendNTPpacket(timeServerIP);
    Serial.print("No NTP response received, attempting to resend NTP packet...\n");
    delay(500);
  }
}
