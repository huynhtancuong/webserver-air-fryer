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
// #include <AsyncElegantOTA.h>


// Replace with your network credentials
const char* ssid = "192";
const char* password = "1234567890";

const char *soft_ap_ssid = "WifiAirFryer";
const char *soft_ap_password = "1234567890";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object

AsyncWebSocket ws("/ws");


/**
 * Struct and Enum for State Machine
*/

enum State_e {
  RUNNING = 0,
  STOPPING,
  COOLING
};

static char* state_string[] = {
  [RUNNING]   = "running",
  [STOPPING]  = "stopping",
  [COOLING]   = "cooling"
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
  STOPPING,
  0,
  0,
  100,
  0
};

#define SAFETY_TEMP_THRESHOLD   80
#define THERMOCOUPLE_PIN        33 //ADC2_CH8
#define FAN_PIN                 23
#define RELAY_PIN               22
#define MINUS_LED_1             27
#define LED_POWER               5
#define BUZZER_PIN              33

/**
 * Action Handlers Function
*/
void updateActionHandler(JSONVar request);
void runActionHandler(JSONVar request);
void stopActionHandler(JSONVar request);

/**
 * RTOS Tasks
*/
void task_Timer(void* parameter); // decrease timeRemaining after 1 minute if it > 0
void task_Heating(void* parameter); // keep temperature at set point
void task_Safety(void* parameter); // turn off if the temperature so high
void task_Cooling(void* parameter); // turn on fan until temperature go below 50*C
void task_UpdateCurrentTemp(void* parameter); // update current temperature


/**
 * Working with hardware
*/
int getCurrentTemperature();
void turnOnFan();
void turnOffFan();
void turnOnHeat();
void turnOffHeat();
void turnOnPowerLed();
void beepBuzzer();

//Get Response
String createResponse(enum State_e state, int timeRemain, int setTemp, int setTime, int currentTemp){
  
  JSONVar json_airFryer;

  json_airFryer["state"] = String(state_string[state]);
  json_airFryer["timeRemain"] = String(timeRemain);
  json_airFryer["setTemp"] = String(setTemp);
  json_airFryer["setTime"] = String(setTime);
  json_airFryer["currentTemp"] = String(currentTemp);

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
  WiFi.mode(WIFI_MODE_APSTA);
  // WiFi.mode(WIFI_MODE_STA);
  // WiFi.beginSmartConfig();
  // while (!WiFi.smartConfigDone()) {
  //   delay(500);
  //   Serial.print(".");
  // }
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  // for (int i = 0; i < 5; i++) {
  //   if (WiFi.status() == WL_CONNECTED) break;
  //   Serial.print('.');
  //   // delay(1000);
  // }
  Serial.println();
  Serial.print("ESP32 IP on the WiFi network: ");
  Serial.println(WiFi.localIP());
  // delay(1000);
  WiFi.softAP(soft_ap_ssid, soft_ap_password);
  Serial.print("ESP32 IP as soft AP: ");
  Serial.println(WiFi.softAPIP());
  // WiFi.mode(WIFI_AP);
  // WiFi.softAP(ssid, password);
  // Serial.println(WiFi.softAPIP());
}

void notifyClients(String response) {
  ws.textAll(response);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    JSONVar request = JSON.parse(message);

    // JSON.typeof(jsonVar) can be used to get the type of the variable
    if (JSON.typeof(request) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }
    if (request.hasOwnProperty("action") && request.hasOwnProperty("setTemperature") && request.hasOwnProperty("setTime")) {
      
      Serial.printf("[INFO] Request received: %s \n", JSON.stringify(request).c_str());

      String action = (const char*) request["action"];

      if (action == "update") updateActionHandler(request);
      if (action == "run")    runActionHandler(request);
      if (action == "stop")   stopActionHandler(request);

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
  initFS();
  initWiFi();


  initWebSocket();
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/").setCacheControl("public, must-revalidate");

  

  // Start server
  server.begin();

  // Set up pins
  pinMode(FAN_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Turn on power LED
  turnOnPowerLed();

  // Start heating logic thread
  xTaskCreate(&task_Timer, "task_Timer", 1024, NULL, 4, NULL);
  xTaskCreate(&task_UpdateCurrentTemp, "task_UpdateCurrentTemp", 1024, NULL, 4, NULL);
  xTaskCreate(&task_Cooling, "task_Cooling", 1024, NULL, 4, NULL);
  xTaskCreate(&task_Heating, "task_Heating", 1024, NULL, 4, NULL);
}

void loop() {
  ws.cleanupClients();
}

int getSetTempFromRequest(JSONVar request) {
  return atoi(request["setTemperature"]);
}

int getSetTimeFromRequest(JSONVar request) {
  return atoi(request["setTime"]);
}

void updateActionHandler(JSONVar request) {
  notifyClients(createResponse(airFryer.state, airFryer.timeRemain, airFryer.setTemp, airFryer.setTime, airFryer.currentTemp));
}

void runActionHandler(JSONVar request) {
  if (airFryer.state == RUNNING) {
    Serial.printf("[INFO] Run action has already been executed.\n");
    return;
  }
  // execute run action
  airFryer.state = RUNNING;
  airFryer.setTemp = getSetTempFromRequest(request) > 165 ? 165 : getSetTempFromRequest(request);
  airFryer.setTime = getSetTimeFromRequest(request);
  airFryer.timeRemain = airFryer.setTime;
  // for debugging
  Serial.printf("[INFO] Run action executing with setTemp = %d, setTime = %d.\n", 
                  airFryer.setTemp, airFryer.setTime);
  // Response to client
  notifyClients(createResponse(airFryer.state, airFryer.timeRemain, 
                              airFryer.setTemp, airFryer.setTime, airFryer.currentTemp));
}



void stopActionHandler(JSONVar request) {
  if (airFryer.state == RUNNING) {
    airFryer.state = COOLING;
    airFryer.timeRemain = 0;

    // For debugging
    Serial.printf("[INFO] Stop action executing. Changed to COOLING state.\n");

    // Response to client
    notifyClients(createResponse(airFryer.state, airFryer.timeRemain, 
                                airFryer.setTemp, airFryer.setTime, airFryer.currentTemp));
  }
}

/**
 * RTOS Tasks Definition
*/

void task_Timer(void* parameter) {
  while(true) {
    vTaskDelay(60000 / portTICK_PERIOD_MS);
    if (airFryer.state == RUNNING) {
      if (airFryer.timeRemain > 0) {
        airFryer.timeRemain--;
      } 
      if (airFryer.timeRemain == 0) {
        airFryer.state = COOLING;
        beepBuzzer();
      }
    }
  }
} 

void task_UpdateCurrentTemp(void* parameter) {
  while(true) {
    vTaskDelay(500 / portTICK_PERIOD_MS);
    airFryer.currentTemp = getCurrentTemperature();
  }
}

void task_Heating(void* parameter) {
  int onOffControlThreshold = 5;
  while(true) {

    int onThreshold = airFryer.setTemp - onOffControlThreshold;
    int offThreshold = airFryer.setTemp + onOffControlThreshold;
    if (airFryer.state == RUNNING) {
      if ((getCurrentTemperature() <= onThreshold)) {
        turnOnHeat();
        turnOnFan();
      }
      if (getCurrentTemperature() >= offThreshold) {
        turnOffHeat();
      }
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}
void task_Safety(void* parameter) {

}
void task_Cooling(void* parameter) {
  
  int safetyTemperature = SAFETY_TEMP_THRESHOLD;

  while(true) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (airFryer.state == COOLING) {
      turnOnFan();
      turnOffHeat();
      if (getCurrentTemperature() < safetyTemperature) {
        airFryer.state = STOPPING;
        turnOffFan();
      }
    }
  }
}

/**
 * Working with hardware
*/

int getCurrentTemperature() {
  int adcValue = analogRead(THERMOCOUPLE_PIN);
  int temp = 0.0418*adcValue - 1;
  return temp;
}

void turnOnFan() {
  digitalWrite(FAN_PIN, HIGH);
  // Serial.println("[INFO] Turned ON Fan.");
}

void turnOffFan() {
  digitalWrite(FAN_PIN, LOW);
  // Serial.println("[INFO] Turned OFF Fan.");
}

void turnOnHeat() {
  digitalWrite(RELAY_PIN, HIGH);
  // Serial.println("[INFO] Turned ON Heating.");
}

void turnOffHeat() {
  digitalWrite(RELAY_PIN, LOW);
  // Serial.println("[INFO] Turned OFF Heating.");
}

void turnOnPowerLed() {
  pinMode(LED_POWER, OUTPUT);
  pinMode(MINUS_LED_1, OUTPUT);
  digitalWrite(LED_POWER, HIGH);
  digitalWrite(MINUS_LED_1, LOW);
}

void beepBuzzer() {
  pinMode(BUZZER_PIN, OUTPUT);
}