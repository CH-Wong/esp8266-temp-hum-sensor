/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

// Import required libraries
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <DHT.h>

#define LED 2  //On board LED

#define DHTTYPE    DHT11     // DHT 11
#define DHTPIN 5     // Digital pin connected to the DHT sensor

DHT dht(DHTPIN, DHTTYPE);

// Replace with your network credentials
const char* ssid = "Halloballo";
const char* password = "Ballohallo";

// Create AsyncWebServer object on port 80
//AsyncWebServer server(80);

float temperature;
float humidity;

float readDHT11Temperature() {
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Convert temperature to Fahrenheit
  //t = 1.8 * t + 32;
  if (isnan(t)) {    
    Serial.println("Failed to read from DHT11 sensor!");
    return 0.0;
  }
  else {
    Serial.println(t);
    digitalWrite(LED,!digitalRead(LED));
    return t;
    
  }
}

float readDHT11Humidity() {
  float h = dht.readHumidity();
  if (isnan(h)) {
    Serial.println("Failed to read from DHT11 sensor!");
    return 0.0;
  }
  else {
    Serial.println(h);
    digitalWrite(LED,!digitalRead(LED));
    return h;
  }
}

String formatBytes(size_t bytes) { // convert sizes in bytes to KB and MB
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  }
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  
  dht.begin(); 


  if (SPIFFS.begin()) {
    Serial.println("SPIFFS started. Contents:");
    {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {                      // List the file system contents
        String fileName = dir.fileName();
        size_t fileSize = dir.fileSize();
        Serial.printf("\tFS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
      }
      Serial.printf("\n");
    }
  
  } 
  else {
    Serial.println("Error mounting the file system");
    return;
  }
  





//  // Connect to Wi-Fi
//  WiFi.begin(ssid, password);
//  Serial.println("Connecting to WiFi..");
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(2000);
//    Serial.println("...");
//  }
//
//  // Print ESP32 Local IP Address
//  Serial.println(WiFi.localIP());
//
//  // Route for root / web page
//  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send(SPIFFS, "/index.html");
//  });
//  
//  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send_P(200, "text/plain", temperature.c_str());
//  });
//  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
//    request->send_P(200, "text/plain", humidity.c_str());
//  });
//
//  // Start server
//  server.begin();

}
 
void loop(){

  temperature = readDHT11Temperature();
  humidity = readDHT11Humidity();
  Serial.println("");

  File datafile = SPIFFS.open("/data.txt", "w+");

  if (!datafile) {
    Serial.println("Error opening file for writing");
    return;
  }
  else {
    datafile.print(temperature);
    datafile.print(',');
    datafile.println(humidity);
    
    Serial.println("File Content:");
    while (datafile.available()) {
      Serial.print(datafile.read());
    }
    datafile.close();
    
    delay(5000);

  }
}
