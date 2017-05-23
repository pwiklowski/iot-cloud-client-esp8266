
#include <WebSocketsClient.h>
#include <WebSockets.h>
#include <ESP8266WiFi.h>

const char* ssid = "ASUS";
const char* password = "pass";
const char* espname = "Test device";  //Shows in router and in OTA-menu

WebSocketsClient webSocket;

bool value = false;
unsigned long pressTime = 0;
bool pinChangedCalled = false;
void setup() {
  
  pinMode(14, OUTPUT); 
  pinMode(12, OUTPUT); 
  pinMode(13, INPUT_PULLUP); 
  Serial.begin(115200);
  WiFi.hostname(espname);
  WiFi.mode(WIFI_STA);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(500);
  }
  Serial.print("Connected!\n");
  webSocket.begin("192.168.1.239", 12345, "/connect");
  webSocket.onEvent(webSocketEvent);
}



void notifyChange(bool val){
  value = val;
  if (val)
    webSocket.sendTXT("{\"name\":\"EventValueUpdate\", \"payload\":{\"uuid\":\"0685B960-736F-46F7-BEC0-9E6CBD61ADC1\", \"resource\": \"/switch\", \"value\":{\"value\": true}}}");
  else
    webSocket.sendTXT("{\"name\":\"EventValueUpdate\", \"payload\":{\"uuid\":\"0685B960-736F-46F7-BEC0-9E6CBD61ADC1\", \"resource\": \"/switch\", \"value\":{\"value\": false}}}");

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
"                \"id\": \"0685B960-736F-46F7-BEC0-9E6CBD61ADC1\",\n"
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
      webSocket.sendTXT("{\"name\":\"RequestAuthorize\", \"payload\": { \"token\": \""+TOKEN+"\", \"name\": \"ESP8266 Demo\", \"uuid\": \"UUID\"}}}");
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);

      parseMessage(payload, length);
      
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      hexdump(payload, length);
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
  
   webSocket.loop(); 
}
