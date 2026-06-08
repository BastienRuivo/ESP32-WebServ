#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#define MAX_HISTORY_COUNT 30
//#define MOCK_HISTORY_DATA
#define MOCK_BALANCE_DATA

#include "WeightData.h"

const char* ssid     = "UrFatNetwork";
const char* password = "DuuDuuD38460";

AsyncWebServer server(80);
AsyncEventSource events("/events");

// Timing variables for the simulation loop
unsigned long lastTime = 0;
const unsigned long timerDelay = 100;

WeightData history[MAX_HISTORY_COUNT];
uint32_t historyCount = 0;

float getRandomFloat(float min, float max) {
    return min + static_cast<float>(rand()) / (static_cast<float>(RAND_MAX / (max - min)));
}

void AddValueToHistory(WeightData value)
{
  if(historyCount < MAX_HISTORY_COUNT)
  {
    history[historyCount++] = value;
  }
  else
  {
    for(int i = 0; i < MAX_HISTORY_COUNT - 1; ++i)
    {
      history[i] = history[i + 1];
    }
    history[MAX_HISTORY_COUNT - 1] = value;
  }
}

void JsonifyHistory(String& output)
{
  JsonDocument doc;
  doc["count"] = historyCount;
  
  JsonArray dataArray = doc["data"].to<JsonArray>();
  for (int i = 0; i < historyCount; i++) {
    JsonObject obj = dataArray.add<JsonObject>();
    WeightData::Jsonify(history[i], obj);
  }
  serializeJson(doc, output);
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

    if (error) {
        Serial.print("Échec du parsing JSON du POST : ");
        Serial.println(error.c_str());
        return;
    }

    WeightData newData{};
    newData.date = doc["date"];
    newData.weight = doc["weight"];
    AddValueToHistory(newData);

    Serial.println("\n--- Data received from balance ---");
    Serial.printf("Data time Epoch (ms) : %llu\n", newData.date);
    Serial.printf("Data weight     : %.2f kg\n", newData.weight);
    Serial.println("----------------------------------------------------\n");
    });

  // Attach the handler
  server.addHandler(&events);

  // Start the server 
  server.begin();

  Serial.println("HTTP server started.");

  
  float delta = getRandomFloat(-5.0f, 5.0f);
  float newWeight = std::min(std::max(history[historyCount - 1].weight + delta, 40.0f), 120.0f);

#ifdef MOCK_HISTORY_DATA
  WeightData data{};
  data.date = millis();
  data.weight = newWeight;
  AddValueToHistory(data);

  for(uint32_t i = 1; i < MAX_HISTORY_COUNT; ++i)
  {
    float delta = getRandomFloat(-5.0f, 5.0f);
    float newWeight = std::min(std::max(history[historyCount - 1].weight + delta, 40.0f), 120.0f);

    WeightData data{};
    data.date = millis();
    data.weight = newWeight;
    AddValueToHistory(data);
  }
#endif
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
    Serial.println("Sending json : " + jsonOutput);
  }
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
