#include <Wire.h>
#include <PCF8574.h>

// กำหนด address ของ PCF8574 (ปกติ 0x20-0x27)
PCF8574 pcf8574(0x26); 

void setup() {
  Serial.begin(9600);
  Wire.begin(4, 5);  // SDA=21, SCL=22 (ค่าเริ่มต้นของ ESP32)
  pcf8574.begin();
}

void loop() {
  // วนตรวจสอบ input ทุกขา (0-7)
  for (uint8_t i = 0; i < 8; i++) {
    int inputValue = pcf8574.read(i);
    Serial.print("Input ");
    Serial.print(i);
    Serial.print(" : ");
    Serial.println(inputValue);
  }
  delay(1000); // อ่านทุก 1 วินาที
}
