#include <ESP8266WiFi.h>

void setup() {
  Serial.begin(9600);
  Serial.println();
  Serial.print("ESP8266 Board MAC Address: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // ไม่ต้องใส่อะไรใน loop ก็ได้
}
