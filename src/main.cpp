/* 
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp32-web-server-websocket-sliders/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <Arduino_JSON.h>
#include <string>

// Replace with your network credentials
const char* ssid = "192";
const char* password = "1234567890";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");
// Set LED GPIO
const int ledPin1 = 12;
const int ledPin2 = 13;
const int ledPin3 = 14;

String message = "";
String sliderValue1 = "0";
String sliderValue2 = "0";
String sliderValue3 = "0";

int dutyCycle1;
int dutyCycle2;
int dutyCycle3;

// setting PWM properties
const int freq = 5000;
const int ledChannel1 = 0;
const int ledChannel2 = 1;
const int ledChannel3 = 2;

const int resolution = 8;

/**
 * Struct and Enum for State Machine
*/

enum State_e {
  RUNNING = 0,
  STOP,
  COOLING,
  HEATING
};

static char* state_string[] = {
  [RUNNING] = "running",
  [STOP]    = "stop",
  [COOLING] = "cooling",
  [HEATING] = "heating"
}; 

struct StateMachine {
  enum State_e state;
  int timeRemain;
  int currentTemp;
  int setTemp;
  int setTime;
};

// Air Fryer State Machine default value
struct StateMachine airFryer = {
  STOP,
  0,
  0,
  100,
  0
};

//Json Variable to Hold Slider Values
JSONVar json_airFryer;

//Get Update Package
String getUpdatePackage(){
  json_airFryer["state"] = String(state_string[airFryer.state]);
  json_airFryer["timeRemain"] = String(airFryer.timeRemain);
  json_airFryer["setTemp"] = String(airFryer.setTemp);
  json_airFryer["setTime"] = String(airFryer.setTime);

  String jsonString = JSON.stringify(json_airFryer);

  return jsonString;
}

// Initialize SPIFFS
void initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
   Serial.println("SPIFFS mounted successfully");
  }
}

// Initialize WiFi
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}

void notifyClients(String response) {
  ws.textAll(response);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    JSONVar json = JSON.parse(message);

    // JSON.typeof(jsonVar) can be used to get the type of the variable
    if (JSON.typeof(json) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }
    if (json.hasOwnProperty("action") && json.hasOwnProperty("setTemperature") && json.hasOwnProperty("setTime")) {
      Serial.print("json[\"action\"] = ");
      Serial.println((const char*) json["action"]);

      if (strcmp((const char*) json["action"], "update") == 0) {
        notifyClients(getUpdatePackage());
      }

      if (strcmp((const char*) json["action"], "heat") == 0) {
        airFryer.setTemp = atoi(json["setTemperature"]);
        airFryer.setTime = atoi(json["setTime"]);
        Serial.printf("setTemp %d | setTime %d", airFryer.setTemp, airFryer.setTime);
        // Serial.print("json[\"setTemperature\"] = ");
        // Serial.println(atoi(json["setTemperature"]));
        notifyClients(getUpdatePackage());
      }

    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}


void setup() {
  Serial.begin(115200);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);
  initFS();
  initWiFi();

  // configure LED PWM functionalitites
  ledcSetup(ledChannel1, freq, resolution);
  ledcSetup(ledChannel2, freq, resolution);
  ledcSetup(ledChannel3, freq, resolution);

  // attach the channel to the GPIO to be controlled
  ledcAttachPin(ledPin1, ledChannel1);
  ledcAttachPin(ledPin2, ledChannel2);
  ledcAttachPin(ledPin3, ledChannel3);


  initWebSocket();
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();

}

void loop() {
  ledcWrite(ledChannel1, dutyCycle1);
  ledcWrite(ledChannel2, dutyCycle2);
  ledcWrite(ledChannel3, dutyCycle3);

  ws.cleanupClients();
}
