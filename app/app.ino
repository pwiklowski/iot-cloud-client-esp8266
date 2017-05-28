#include <ESP8266HTTPClient.h>

#include"FS.h"
#include <DNSServer.h>            
#include <ESP8266WebServer.h>     
#include <WiFiManager.h>          
#include <ArduinoJson.h>

#include <WebSocketsClient.h>
#include <WebSockets.h>

#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>

const char* NAME = "Wiklosoft Smart Device"; 
String deviceName = "Smart Light";

String HUB_UUID = "f224e10b-1a08-4943-b9ae-6f4a8844be6f";
String DEVICE_UUID = "14ad5651-1e97-49c5-8a7d-b8cd257af5dc";

WebSocketsClient webSocket;

bool value = false;
unsigned long pressTime = 0;
bool pinChangedCalled = false;

String access_token = "empty";
String NAME_FILENAME = "name.txt";


ESP8266WebServer server(80);

void setup() {
  Serial.begin(115200);
  SPIFFS.begin();
  
  deviceName = readString(NAME_FILENAME);  
  
  pinMode(14, OUTPUT); 
  pinMode(12, OUTPUT); 
  pinMode(13, INPUT_PULLUP); 

  
  WiFiManager wifiManager;
  if (digitalRead(13) == LOW){
    Serial.print("Reset wifi settings!\n");
    wifiManager.resetSettings();
  }
  wifiManager.autoConnect();

  Serial.print("Connected!\n");

  refreshTokens();
  
  webSocket.onEvent(webSocketEvent);
  webSocket.begin("192.168.1.239", 12345, "/connect");
  ArduinoOTA.setHostname(deviceName.c_str());
  ArduinoOTA.begin();

  server.on("/code", handleCode);
  server.on("/name", handleName);
  server.begin();

}

void handleCode() {
  if (server.method() == HTTP_POST){
    String code = server.arg("code");

    getTokens(code);


    server.send(200, "text/plain", "ok");
  }else{
    server.send(503, "text/plain", "failed");
  }
}

void handleName() {
  if (server.method() == HTTP_POST){
    deviceName = server.arg("name");
    saveString(NAME_FILENAME, deviceName);
    server.send(200, "text/plain", "ok");
    ESP.restart();
  }else{
    server.send(503, "text/plain", "failed");
  }
}


void saveString(String filename, String data){
  File fsUploadFile = SPIFFS.open(filename, "w");
  fsUploadFile.write((const uint8_t*)data.c_str(), data.length());
  fsUploadFile.close();
}
String readString(String filename){
  String res;
  bool exist = SPIFFS.exists(TOKEN_FILENAME);
  if (exist){
    File fsUploadFile = SPIFFS.open(filename, "r");
    res = fsUploadFile.readString();
    fsUploadFile.close();
  }

  return res;
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

  if (eventName == "RequestGetDevices"){
    Serial.print("devices requested!\n");
     webSocket.sendTXT("{\n"
"    \"name\": \"EventDeviceListUpdate\",\n"
"    \"payload\": {\n"
"        \"devices\": [\n"
"            {\n"
"                \"id\": \""+DEVICE_UUID+"\",\n"
"                \"name\": \""+deviceName+"\",\n"
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
      webSocket.sendTXT("{\"name\":\"RequestAuthorize\", \"payload\": { \"token\": \""+access_token+"\", \"name\": \""+NAME+"\", \"uuid\": \""+HUB_UUID+"\"}}}");
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      parseMessage(payload, length);
      break;
  }
}

void refreshTokens(){
  String refreshToken = readString("refresh_token.txt");

  
  String host = "auth.wiklosoft.com";
  
  WiFiClientSecure client;
  if (!client.connect(host.c_str(), 443)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/v1/oauth/tokens";

  String body = "grant_type=refresh_token&refresh_token="+refreshToken;

  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + host);
  client.print("Content-Length: ");
  client.println(body.length()); 
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Authorization: Basic aHViX2NsaWVudDpodWJfY2xpZW50X3NlY3JldA==");
  client.println();
  client.println(body);
               

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
Serial.println(line);
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(line);
  String token = root["access_token"];
  access_token = token;
  String refresh_token = root["refresh_token"];
  
  saveString("access_token.txt", access_token);
  saveString("refresh_token.txt", refresh_token);
}
 
void getTokens(String code){
  String host = "auth.wiklosoft.com";
  
  WiFiClientSecure client;
  if (!client.connect(host.c_str(), 443)) {
    Serial.println("connection failed");
    return;
  }

  String url = "/v1/oauth/tokens";

  String body = "grant_type=authorization_code&code="+code+"&redirect_uri=https://www.example.com";

  client.println("POST " + url + " HTTP/1.1");
  client.println("Host: " + host);
  client.print("Content-Length: ");
  client.println(body.length()); 
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.println("Authorization: Basic aHViX2NsaWVudDpodWJfY2xpZW50X3NlY3JldA==");
  client.println();
  client.println(body);
               

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
Serial.println(line);
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(line);
  String token = root["access_token"];
  access_token = token;
  String refresh_token = root["refresh_token"];
  
  saveString("access_token.txt", access_token);
  saveString("refresh_token.txt", refresh_token);
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
  server.handleClient();
}
