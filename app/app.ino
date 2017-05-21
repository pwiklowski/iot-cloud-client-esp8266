#include <ESP8266WiFi.h>

const char* ssid = "ASUS";
const char* password = "pass";
const char* espname = "Test device";  //Shows in router and in OTA-menu

void setup() {
  Serial.begin(115200);
  WiFi.hostname(espname);
  WiFi.mode(WIFI_STA);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(500);
  }
  Serial.print("Connected!\n");
}

void loop() {
  Serial.print("HI!\n");
  delay(1000);
}
