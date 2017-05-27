#include <DNSServer.h>            
#include <ESP8266WebServer.h>     
#include <WiFiManager.h>          

#include <WebSocketsClient.h>
#include <WebSockets.h>

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

const char* ssid = "ASUS";
const char* password = "pass";
const char* espname = "Test device";  //Shows in router and in OTA-menu
const char* NAME = "Wiklosoft Smart Device"; 

String HUB_UUID = "f224e10b-1a08-4943-b9ae-6f4a8844be6f";
String DEVICE_UUID = "14ad5651-1e97-49c5-8a7d-b8cd257af5dc";

WebSocketsClient webSocket;

bool value = false;
unsigned long pressTime = 0;
bool pinChangedCalled = false;
void setup() {
  
  pinMode(14, OUTPUT); 
  pinMode(12, OUTPUT); 
  pinMode(13, INPUT_PULLUP); 
  Serial.begin(115200);
  
  WiFiManager wifiManager;
  if (digitalRead(13) == LOW){
    Serial.print("Reset wifi settings!\n");
    wifiManager.resetSettings();
  }
  wifiManager.autoConnect();

  Serial.print("Connected!\n");
  webSocket.begin("192.168.1.239", 12345, "/connect");
  webSocket.onEvent(webSocketEvent);

  ArduinoOTA.setHostname(NAME);
  ArduinoOTA.begin();
}



void notifyChange(bool val){
  value = val;
  if (val)
    webSocket.sendTXT("{\"name\":\"EventValueUpdate\", \"payload\":{\"uuid\":\""+DEVICE_UUID+"\", \"resource\": \"/switch\", \"value\":{\"value\": true}}}");
  else
    webSocket.sendTXT("{\"name\":\"EventValueUpdate\", \"payload\":{\"uuid\":\""+DEVICE_UUID+"\", \"resource\": \"/switch\", \"value\":{\"value\": false}}}");

  digitalWrite(14, val ? HIGH : LOW);
}

void parseMessage(uint8_t* payload, size_t len){
  StaticJsonBuffer<500> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(payload);

  String eventName = root["name"];
  //Serial.print("Event %s\n", eventName);

//TODO: use ESP.getChipId() to set id
  
  if (eventName == "RequestGetDevices"){
    Serial.print("devices requested!\n");
     webSocket.sendTXT("{\n"
"    \"name\": \"EventDeviceListUpdate\",\n"
"    \"payload\": {\n"
"        \"devices\": [\n"
"            {\n"
"                \"id\": \""+DEVICE_UUID+"\",\n"
"                \"name\": \"ESP Light \",\n"
"                \"variables\": [\n"
"                    {\n"
"                        \"href\": \"/switch\",\n"
"                        \"if\": \"oic.if.rw\",\n"
"                        \"n\": \"Switch\",\n"
"                        \"rt\": \"oic.r.switch.binary\",\n"
"                        \"values\": {\n"
"                            \"rt\": \"oic.r.switch.binary\",\n"
"                            \"value\": 1\n"
"                        }\n"
"                    }\n"
"                ]\n"
"            }\n"
"]}}");
  }else if(eventName =="RequestSetValue"){
    bool val = root["payload"]["value"]["value"];
    Serial.printf("set value %d!\n", (int)val);
    notifyChange(val);
  }
  
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected to url: %s\n",  payload);
      webSocket.sendTXT("{\"name\":\"RequestAuthorize\", \"payload\": { \"token\": \""+TOKEN+"\", \"name\": \""+NAME+"\", \"uuid\": \""+HUB_UUID+"\"}}}");
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      parseMessage(payload, length);
      break;
  }
}


void loop() {
  if (digitalRead(13) == LOW){
    if (pressTime == 0) pressTime = millis();

    if (!pinChangedCalled && millis() - pressTime > 100) {
      notifyChange(!value);
      pinChangedCalled = true;
    }
  }else{
    pressTime = 0;
    pinChangedCalled = false;
  }
  ArduinoOTA.handle();
   webSocket.loop(); 
}
