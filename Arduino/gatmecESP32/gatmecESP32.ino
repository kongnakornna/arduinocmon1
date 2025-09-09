#include <WiFi.h>

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println();

  // อ่าน MAC Address ของ WiFi STA (Station)
  String macAddress = WiFi.macAddress();
  Serial.print("ESP32 MAC Address: ");
  Serial.println(macAddress);
}

void loop() {
  // ไม่ต้องทำอะไรใน loop
}
