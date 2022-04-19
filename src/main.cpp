#include <Arduino.h>
#include <ESP8266WiFi.h>        // Include the Wi-Fi library
#include <ESP8266mDNS.h>        // Include the mDNS library
#include <ESP8266WebServer.h>

#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <LittleFS.h>   // Include the LittleFS library
#include <DHTesp.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include <addons/TokenHelper.h>


#define DHTTYPE DHT11     // DHT 11
#define DHTPIN 2     // GPIO pin out, not Digital pin number

DHTesp dht;

// WiFi variable Declaration 
#define WIFI_SSID "Halloballo"
#define WIFI_PASSWORD "Ballohallo"
#define MDNS_HOSTNAME "livingroom"// mDNS hostname 

// OTA variable declaration
#define OTAPassword "IWantToUpload"

const int intervalMeasurement = 2000;   // Do a temperature measurement every minute

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// For Network Time Protocol (NTP)
WiFiUDP UDP;
IPAddress timeServerIP;          // time.nist.gov NTP server address
const char* NTPServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48;  // NTP time stamp is in the first 48 bytes of the message
byte NTPBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets


/* 2. Define the API Key */
#define API_KEY "AIzaSyDPOk299hwHKRP4F6M5hFwyVtNVhQm-KP0"
#define SERVICE_ACCOUNT_FILENAME "/private-key.json"

/* 3. Define the RTDB URL */
#define DATABASE_URL "bidoof-home-database-default-rtdb.europe-west1.firebasedatabase.app" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app



// Init Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// Server variable declaration
ESP8266WebServer server(80);
String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)

// LittleFS variable declaration
File fsUploadFile;

// NTP variable declaration
#define ONE_HOUR 3600000UL
const unsigned long intervalNTP = ONE_HOUR; // Update the time every hour
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();

unsigned long prevMeasurement = 0;
uint32_t timeUNIX = 0;                      // The most recent timestamp received from the time server

void initOTA() {
  // OTA INIT
  ArduinoOTA.setHostname(MDNS_HOSTNAME);
  ArduinoOTA.setPassword(OTAPassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

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


  // MDNS INIT
  if (!MDNS.begin(MDNS_HOSTNAME)) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");
  
  Serial.print("Open http://");
  Serial.print(MDNS_HOSTNAME);
  Serial.println(".local/edit to see the file browser");
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

    /** Assign the unique user ID (uid) (required)
     * This uid will be compare to the auth.uid variable in the database rules.
     *
     * If the assigned uid (user UID) was not existed, the new user will be created.
     *
     * If the uid is empty or not assigned, the library will create the OAuth2.0 access token
     * instead.
     *
     * With OAuth2.0 access token, the device will be signed in as admin which has
     * the full ggrant access and no database rules and custom claims are applied.
     * This similar to sign in using the database secret but no admin rights.
     */

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    Firebase.reconnectWiFi(true);

    fbdo.setResponseSize(4096);

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

    /** Assign the maximum retry of token generation */
    config.max_token_generation_retry = 5;

    Firebase.begin(&config, &auth);
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


bool handleFileRead(String path){  // send the right file to the client (if it exists)
  // Print the filename request in serial monitor
  Serial.println("handleFileRead: " + path);
  // If a folder is requested, send the index.html file instead as is standard for websites
  if(path.endsWith("/")) path += "index.html";           // If a folder is requested, send the index file

  // Get the MIME type from the path name
  String contentType = getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  
  if(LittleFS.exists(pathWithGz) || LittleFS.exists(path)){  // If the file exists, either as a compressed archive, or normal
    if(LittleFS.exists(pathWithGz))                          // If there's a compressed version available
      path += ".gz";                                         // Use the compressed version
    File file = LittleFS.open(path, "r");                    // Open the file
    file.close();                                          // Close the file again
    Serial.println(String("\tSent file: ") + path);
    return true;
  }
  Serial.println(String("\tFile Not Found: ") + path);
  return false;                                          // If the file doesn't exist, return false
}

void handleFileUpload(){
  if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = LittleFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
  }
}

void handleFileDelete(){
  if(server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileDelete: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(!LittleFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  LittleFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileCreate(){

  if(server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  Serial.println("handleFileCreate: " + path);
  if(path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if(LittleFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = LittleFS.open(path, "w");
  if(file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}

void handleFileList() {
  
  if(!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
  
  String path = server.arg("dir");
  Serial.println("handleFileList: " + path);
  Dir dir = LittleFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  
  output += "]";
  server.send(200, "text/json", output);
}

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".csv")) return "application/csv";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void initSPIFFS() {
  //LittleFS INIT
  LittleFS.begin();                           // Start the SPI Flash Files System
  {
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    Serial.printf("\n");
  }
}

void initServer(){
  // SERVER INIT
  //called when the url is not defined here
  //use it to load content from LittleFS
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });
  
  //list directory
  server.on("/list", HTTP_GET, handleFileList);
  //load editor
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //create file
  server.on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  server.on("/edit", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleFileUpload);

  //get heap status, analog input value and all GPIO statuses in one json call
  server.on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
  Serial.println("HTTP server started");
}

void initDisplay() {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
  Serial.println(F("SSD1306 allocation failed"));
  for(;;);
  }
  delay(2000);
}

void setup() {
  // BOARD INIT
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');

  initOTA();
  initWiFi();
  initNTP();
  initSPIFFS();
  initServer();
  initDisplay();

  initFirebase();
  
  dht.setup(DHTPIN, DHTesp::DHT11); // Connect DHT sensor to GPIO 17

  delay(100);
}


void loop(void) {
 
  ArduinoOTA.handle();
  server.handleClient();                    // Listen for HTTP requests from clients

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

    if (currentMillis - prevMeasurement > intervalMeasurement) { // 750 ms after requesting the temperature
      prevMeasurement = currentMillis;
      uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
      // The actual time is the last NTP time plus the time that has elapsed since the last NTP response

      float temp = dht.getTemperature();
      float hum = dht.getHumidity();
      if (isnan(temp) ||  isnan(hum)) {
        temp = 0.0;
        hum = 0.0;
      }
      
      temp = round(temp * 100.0) / 100.0; // round temperature to 2 digits
      hum = round(hum * 100.0) / 100.0; // round temperature to 2 digits


      Serial.print("Measurement data: ");
      Serial.print(actualTime);
      Serial.print("\t");
      Serial.print(temp);
      Serial.print("\t");
      Serial.println(hum);


      Serial.printf("Pushing Temperature Data %s\n", Firebase.RTDB.pushInt(&fbdo, F("/livingroom/temperature"), 2) ? "ok" : fbdo.errorReason().c_str());
      Serial.printf("Pushing Humidity Data %s\n", Firebase.RTDB.pushInt(&fbdo, F("/livingroom/humidity"), 2) ? "ok" : fbdo.errorReason().c_str());

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(0, 0);  

      display.print("http://");
      display.print(MDNS_HOSTNAME);
      display.println(".local/");
      display.println(WiFi.localIP());
      display.print("\n");
      
      static char strBuffer[10];
      char tempPrint[20];
      dtostrf(temp, 2, 2, strBuffer);
      sprintf(tempPrint, "Temperature: %s'C\n", strBuffer);
      display.println(tempPrint);
    
      char humPrint[16];
      dtostrf(hum, 2, 1, strBuffer);
      sprintf(humPrint, "Humidity: %s %%", strBuffer);
      display.println(humPrint);
      
      display.display(); 
      
    }
  } 
  else {                                    // If we didn't receive an NTP response yet, send another request
    sendNTPpacket(timeServerIP);
    Serial.print("No NTP response received, attempting to resend NTP packet...\n");
    delay(500);
  }
}
