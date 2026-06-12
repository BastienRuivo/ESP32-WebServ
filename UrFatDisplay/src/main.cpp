#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define MAX_HISTORY_COUNT 30
#define MOCK_BALANCE_DATA

#include "WeightData.h"
#include "RingBuffer.h"

const char* ssid     = "UrFatNetwork";
const char* password = "DuuDuuD38460";
const char* HISTORY_FILENAME = "/WeightData.json";

AsyncWebServer server(80);
AsyncEventSource events("/events");

// Timing variables for the simulation loop
unsigned long lastTime = 0;
const unsigned long timerDelay = 100;

RingBuffer<WeightData, MAX_HISTORY_COUNT> history;

void LoadHistoryFromMemory()
{
  history.clear();
  Serial.println("Loading History.");

  if (!LittleFS.exists(HISTORY_FILENAME)) 
  {
    Serial.println("No history file found. Initializing empty file.");
    File file = LittleFS.open(HISTORY_FILENAME, FILE_WRITE);
    if (file) 
    { 
      file.print("[]"); 
      file.close(); 
    }
    return;
  }

  if (!LittleFS.exists(HISTORY_FILENAME)) 
  {
    Serial.println("No history file");
    return;
  }

  File file = LittleFS.open(HISTORY_FILENAME, FILE_READ);
  if (!file) 
  {
    Serial.println("Error opening history file for reading.");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) 
  {
    Serial.printf("Failed to parse JSON backup file: %s\n", error.c_str());
    return;
  }

  JsonArray array = doc.as<JsonArray>();
    
  size_t size = array.size();
  Serial.println("History disk size");
  Serial.println(size);

  for (int i = 0; i < size; ++i)
  {

    if(size - i >= MAX_HISTORY_COUNT) continue;
    WeightData data{};
    data.date = array[i]["date"];
    data.weight = array[i]["weight"];
    history.push_back(data);

    Serial.println("\n--- Data from history ---");
    Serial.printf("Current epoch time: %lld\n", (long long)data.date);
    Serial.printf("Data weight     : %.2f kg\n", data.weight);
    Serial.println("----------------------------------------------------\n");
  }

  Serial.printf("History populated !");
}

bool UploadDataToMemory(WeightData& data) 
{
  File file = LittleFS.open(HISTORY_FILENAME, "r+"); // Open read write wiuth r+
  if (!file) 
  {
    Serial.println("Failed to open file for uploading.");
    return false;
  }

  size_t fileSize = file.size();

  // Check if the file is completely empty or corrupted
  if (fileSize < 2) 
  {
    file.close();
    file = LittleFS.open(HISTORY_FILENAME, FILE_WRITE);
    file.print("[]");
    fileSize = file.size();
  }

  // Move the file write pointer right before the closing bracket ']'
  file.seek(fileSize - 1, SeekSet);

  // If the file already contains entries, inject a comma separator
  if (fileSize > 2) 
  {
    file.print(",");
  }

  // Serialize the new point
  JsonDocument doc;
  WeightData::Jsonify(data, doc);

  serializeJson(doc, file);

  // Reclose the json array
  file.print("]");
  file.close();
  
  return true;
}

void addDataToHistory(WeightData& data) 
{
  history.push_back(data);
  UploadDataToMemory(data);
}

float getRandomFloat(float min, float max) 
{
  return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

void JsonifyHistory(String& output)
{
  JsonDocument doc;
  doc["count"] = history.size();
  
  JsonArray dataArray = doc["data"].to<JsonArray>();
  for (int i = 0; i < history.size(); i++) {
    JsonObject obj = dataArray.add<JsonObject>();
    WeightData::Jsonify(history[i], obj);
  }
  serializeJson(doc, output);
}

void SendBalanceData(WeightData data)
{
  // if someone is listening
  if(events.count() > 0) 
  {  
    JsonDocument doc;
    WeightData::Jsonify(data, doc);

    String jsonOutput;
    serializeJson(doc, jsonOutput);
    events.send(jsonOutput.c_str(), "sensor-addPoint", data.date);
  }
}

void setup() {
  // monitor speed to check data
  Serial.begin(115200);

  // Check if filesystem is working to load the .html file
  if(!LittleFS.begin(true))
  {
      Serial.println("An Error has occurred while mounting LittleFS");
      return;
  }

  Serial.print("Setting up Access Point... ");
  if (WiFi.softAP(ssid, password)) 
  {
    Serial.println("Ready!");
  } 
  else 
  {
    Serial.println("Failed!");
    return;
  }

  LoadHistoryFromMemory();

  // By default, the ESP32 AP IP address is 192.168.4.1
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // Configure Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(LittleFS, "/main.html", "text/html");
  });

  server.on("/history", HTTP_GET, [](AsyncWebServerRequest *request){
    String output;
    JsonifyHistory(output);
    request->send(200, "application/json", output);
  });

  server.on("/submit-data", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "OK");
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, data, len);

    String output;
    serializeJson(doc, output);
    Serial.println(output.c_str());

    if (error) 
    {
        Serial.print("Échec du parsing JSON du POST : ");
        Serial.println(error.c_str());
        return;
    }

    WeightData newData{};
    newData.date = doc["date"];
    newData.weight = doc["weight"];
    addDataToHistory(newData);

    Serial.println("\n--- Data received from balance ---");
    Serial.printf("Current epoch time: %lld\n", (long long)newData.date);
    Serial.printf("Data weight     : %.2f kg\n", newData.weight);
    Serial.println("----------------------------------------------------\n");
    });

  // Attach the handler
  server.addHandler(&events);

  // Start the server 
  server.begin();

  Serial.println("HTTP server started.");
}

void loop() 
{
  #ifdef MOCK_BALANCE_DATA
  if ((millis() - lastTime) > timerDelay) 
  {
    float newWeight = getRandomFloat(40.0f, 120.0f);

    WeightData data{};
    data.date = 0;
    data.weight = newWeight;
    
    SendBalanceData(data);
    
    lastTime = millis();
  }
  #endif
}
