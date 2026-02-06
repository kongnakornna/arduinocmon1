#include <SPI.h>
#include <PubSubClient.h>
#include <ETH.h>
#include "esp_task_wdt.h"  // Watchdog สำหรับ ESP32

#define working_led  32

// กำหนดพินและ config Ethernet
#define ETH_ADDR        0
#define ETH_POWER_PIN  -1
#define ETH_MDC_PIN    23
#define ETH_MDIO_PIN   18
#define ETH_TYPE       ETH_PHY_LAN8720
#define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT

// IP Address ค่าคงที่
IPAddress local_ip(172, 25, 99, 50);
IPAddress subnet(255, 255, 255, 240);
IPAddress gateway(172, 25, 99, 62);
IPAddress dns(8, 8, 8, 8);

// MQTT Settings
const char* dataTopic = "CMONHW01/DATA";
const char* controlTopic = "CMONHW01/CONTROL";
const char* mqttServer = "172.25.99.60";      // ใช้ IP local MQTT Broker
const int mqttPort = 1883;

WiFiClient ethClient;
PubSubClient mqttClient(ethClient);

unsigned long lastMillis = 0;
const long interval = 5000;  // 5 วินาที สำหรับแสดงข้อความสถานะ "Connect OK"

unsigned long lastPublishMillis = 0;
const long publishInterval = 3000;  // 3 วินาที สำหรับส่งข้อความ "hello"

const char* publishTopic = "CMONHW01/CMONHW01";

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32Client")) {
      Serial.println("connected");
      // subscribe topic ถ้าต้องการ
      mqttClient.subscribe(controlTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(working_led, OUTPUT);

  // เริ่มต้น Ethernet
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
  if (!ETH.config(local_ip, gateway, subnet, dns, dns)) {
    Serial.println("LAN8720 Configuration failed.");
  } else {
    Serial.println("LAN8720 Configuration success.");
    Serial.print("IP Address: ");
    Serial.println(ETH.localIP());
  }

  mqttClient.setServer(mqttServer, mqttPort);

  // ตั้งค่า watchdog timeout 10 วินาที
  esp_task_wdt_init(10, true);
  esp_task_wdt_add(NULL); // เพิ่ม Task ปัจจุบันเข้า Watchdog
}

void loop() {
  // รีเซ็ต watchdog ทุก loop
  esp_task_wdt_reset();

  if (!mqttClient.connected()) {
    Serial.println("Connect loss");
    digitalWrite(working_led, LOW);   // LED off แสดงสถานะขาดการเชื่อมต่อ
    reconnect();
  } else {
    digitalWrite(working_led, HIGH);  // LED on แสดงสถานะเชื่อมต่อได้
    unsigned long now = millis();

    // แสดงข้อความสถานะ "Connect OK" ทุก 5 วินาที
    if (now - lastMillis >= interval) {
      lastMillis = now;
      Serial.println("Connect OK");
    }

    // ส่งข้อความ "hello" ทุก 3 วินาที ไปยัง topic "CMONHW01/CMONHW01"
    if (now - lastPublishMillis >= publishInterval) {
      lastPublishMillis = now;
      if (mqttClient.publish(publishTopic, "hello")) {
        Serial.println("Sent: hello");
      } else {
        Serial.println("Send failed!");
      }
    }

    mqttClient.loop();
  }
}
