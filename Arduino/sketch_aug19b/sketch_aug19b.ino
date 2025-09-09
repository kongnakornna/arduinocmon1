#include <SPI.h>
#include <EthernetENC.h>

// กำหนด MAC Address ที่ใช้สำหรับ ENC28J60 (ควรต่างจาก MAC ของ ESP32 WiFi)
byte mac[] = {0x08, 0xF3, 0xDA, 0x54, 0x2B, 0x68};      // MAC Address ที่ตั้งเอง
IPAddress ip(192, 168, 1, 99);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 1, 1);

EthernetClient ethClient;

void setup() {
  Serial.begin(9600);
  delay(1000);

  Ethernet.begin(mac, ip, gateway, subnet);

  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());
}

void loop() {
  // ตัวอย่าง loop ว่างเพื่อทดสอบการเชื่อมต่อ Ethernet
}
