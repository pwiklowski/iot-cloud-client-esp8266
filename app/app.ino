#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <ESP8266mDNS.h>
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
String SSID_FILENAME = "ssid.txt";
String PASS_FILENAME = "pass.txt";

String ACCESS_TOKEN_FILENAME = "access_token.txt";
String REFRESH_TOKEN_FILENAME = "refresh_token.txt";

String ssid;
String pass;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

void setup() {
  Serial.begin(115200);
  SPIFFS.begin();
  
  deviceName = readString(NAME_FILENAME);  

  ssid = readString(SSID_FILENAME);
  pass = readString(PASS_FILENAME);
  
  pinMode(14, OUTPUT); 
  pinMode(12, OUTPUT); 
  pinMode(13, INPUT_PULLUP); 
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  delay(100);

  if (digitalRead(13) == LOW || ssid.length() == 0 || pass.length() == 0){
    factoryReset();
    Serial.println("Start AP mode");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Wiklosoft Smart Device");
    IPAddress accessIP = WiFi.softAPIP();
    Serial.print("ESP AccessPoint IP address: ");
    Serial.println(accessIP);
    server.on("/name", handleName);
    server.on("/wifi", handleWifi); 
    server.on("/code", handleCode);
    server.on("/restart", handleRestart); 

  }else{
    Serial.println("Start Client mode");
    WiFi.begin(ssid.c_str(), pass.c_str());
  
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }

    refreshTokens();
    webSocket.onEvent(webSocketEvent);
    webSocket.begin("192.168.1.239", 12345, "/connect");
    
    ArduinoOTA.setHostname(deviceName.c_str());
    ArduinoOTA.begin();
    MDNS.addService("wiklosoftconfig", "tcp", 80);
    MDNS.addServiceTxt("wiklosoftconfig", "tcp", "uuid", DEVICE_UUID);
    httpUpdater.setup(&server);
  }
  Serial.print("Connected!\n");
  server.on("/uuid", handleUuid);

  server.begin();
}

void factoryReset(){
  saveString(ACCESS_TOKEN_FILENAME, "");
  saveString(REFRESH_TOKEN_FILENAME, "");
  saveString(NAME_FILENAME, "Wiklosoft Smart Device");
  saveString(SSID_FILENAME, "");
  saveString(PASS_FILENAME, "");
}

void handleCode() {
  if (server.method() == HTTP_POST){
    String access_token = server.arg("access_token");
    String refresh_token = server.arg("refresh_token");
    
    saveString(ACCESS_TOKEN_FILENAME, access_token);
    saveString(REFRESH_TOKEN_FILENAME, refresh_token);
    Serial.println("handleCode");
    Serial.println(access_token);
    Serial.println(refresh_token);
    

    server.send(200, "text/plain", "ok");
  }else{
    String token = readString(ACCESS_TOKEN_FILENAME);

    if (token == ""){
      server.send(404, "text/plain", "failed");
    }else{
      server.send(200, "text/plain", "ok");
    }
    
    
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


void handleUuid() {
  server.send(200, "text/plain", DEVICE_UUID);
}
void handleWifi() {
  if (server.method() == HTTP_POST){
    ssid = server.arg("ssid");
    pass = server.arg("password");
    Serial.println("handleName");
    Serial.println(ssid);
    Serial.println(pass);
    
    saveString(SSID_FILENAME, ssid);
    saveString(PASS_FILENAME, pass);
    server.send(200, "text/plain", "ok");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
     while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    MDNS.begin(deviceName.c_str());
    MDNS.addService("wiklosoftconfig", "tcp", 80);
    MDNS.addServiceTxt("wiklosoftconfig", "tcp", "uuid", DEVICE_UUID);
    
  }else{
    server.send(200, "text/plain", ssid);
  }
}
void handleRestart() {
  Serial.print("Restart!\n");
  ESP.restart();
}
void saveString(String filename, String data){
  File fsUploadFile = SPIFFS.open(filename, "w");
  fsUploadFile.write((const uint8_t*)data.c_str(), data.length());
  fsUploadFile.close();
}
String readString(String filename){
  String res;
  bool exist = SPIFFS.exists(filename);
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
  String refreshToken = readString(REFRESH_TOKEN_FILENAME);

  
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
  
  saveString(ACCESS_TOKEN_FILENAME, access_token);
  saveString(REFRESH_TOKEN_FILENAME, refresh_token);
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
