#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_task_wdt.h>  // สำหรับ Watchdog ESP32

// กำหนดค่า Network
byte mac[] = { 0x80, 0xF3, 0xDA, 0x4B, 0x21, 0x4C };
IPAddress ip(192, 168, 72, 6);
IPAddress gateway(192, 168, 72, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dnsIP(8, 8, 8, 8);

// MQTT broker
//const char* mqttServer = "broker.hivemq.com";
const char* mqttServer = "192.168.72.70";
const int mqttPort = 1883;

const char* dataTopic = "BAACTW22/DATA";
const char* controlTopic = "BAACTW22/CONTROL";

EthernetClient ethClient;
PubSubClient client(ethClient);

const int mVperAmp = 100;

float Analog1;
float Analog2;
float AnalogMin = 300;   //ค่าน้อยกว่า x = power off
float AnalogMax = 350;   //ค่ามากว่า x =  power on
float AnalogOver1 = 1000;  //ค่าสูงเกินกำหนด  Overload
float AnalogOver2 = 2500;  //ค่าสูงเกินกำหนด  Overload

float vpp1, vpp2;
float temp = 25.5;
float tempBuffer;
int ContRelay1 = 0;
int ActRelay1 = 0;
int ActFan1 = 0;
int AlarmFan1 = 1;
int Fan1_Overload = 0;

int ContRelay2 = 0;
int ActRelay2 = 0;
int ActFan2 = 0;
int AlarmFan2 = 1;
int Fan2_Overload = 0;

int Mqtt_status = 0;

// ****************** Pin Definitions *************New*******
#define RELAY1_PIN   25
#define RELAY2_PIN   26
#define ALARM1_PIN   27
#define ALARM2_PIN   14
#define Analog1_PIN  34
#define Analog2_PIN  35
#define ONE_WIRE_BUS 4
#define ONBOARD_LED  2
#define POWER_LED    33

#define Ct1          22
#define Ct2          21

// ****************** Sensor Objects *********************
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// ****************** Variables **************************
unsigned long previousMillis = 0;
const long interval = 10000;

// --- สำหรับ LED กระพริบทุก 0.5 วินาที ---
unsigned long previousLedMillis = 0;
const long ledInterval = 500;
bool ledState = false;

// --- สำหรับ MQTT กระพริบทุก 5 วินาที ---
unsigned long previousMQTTMillis = 0;
const long MQTTInterval = 5000;

unsigned long lastPublish = 0;
const unsigned long publishInterval = 3000; // 3 วินาที

void messageReceived(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == controlTopic) {
    if (message == "0") {
      ContRelay1 = 0;
      digitalWrite(RELAY1_PIN, HIGH);
      Serial.println("FAN1 : OFF");
    } else if (message == "1") {
      ContRelay1 = 1;
      digitalWrite(RELAY1_PIN, LOW);
      Serial.println("FAN1 : ON");
    } else if (message == "2") {
      ContRelay2 = 0;
      digitalWrite(RELAY2_PIN, HIGH);
      Serial.println("FAN2 : OFF");
    } else if (message == "3") {
      ContRelay2 = 1;
      digitalWrite(RELAY2_PIN, LOW);
      Serial.println("FAN2 : ON");
    }
  }
}

// ฟังก์ชันเชื่อมต่อ MQTT พร้อม exponential backoff
void connectMQTT() {
  int retryCount = 0;
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      Mqtt_status = 1;
      client.subscribe(controlTopic);
      digitalWrite(POWER_LED, LOW);
      retryCount = 0;
      break;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      retryCount++;
      int delayTime = 3000 * retryCount; // exponential backoff
      if (delayTime > 30000) delayTime = 30000; // จำกัดสูงสุด 30 วินาที
      Serial.print(" retrying in ");
      Serial.print(delayTime);
      Serial.println(" ms");
      Mqtt_status = 0;
      digitalWrite(POWER_LED, HIGH);
      vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
    esp_task_wdt_reset(); // feed watchdog ในขณะรอ
  }
}

void setup() {
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(ALARM1_PIN, OUTPUT);
  pinMode(ALARM2_PIN, OUTPUT);
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(POWER_LED, OUTPUT);
  pinMode(Ct1, INPUT);
  pinMode(Ct2, INPUT);

  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(ALARM1_PIN, HIGH);
  digitalWrite(ALARM2_PIN, HIGH);
  digitalWrite(ONBOARD_LED, HIGH);
  digitalWrite(POWER_LED, HIGH);

  digitalWrite(Ct1, HIGH);
  digitalWrite(Ct2, HIGH);

  Serial.begin(9600);

  Ethernet.begin(mac, ip, dnsIP, gateway, subnet);
  delay(1500);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(messageReceived);

  tempSensor.begin();

  Serial.println("System Ready");

  esp_task_wdt_init(10, true);   // 10 วินาที timeout, auto reset
  esp_task_wdt_add(NULL);         // เพิ่ม task หลัก เข้า watchdog
}

void loop() {
  esp_task_wdt_reset();  // feed watchdog

  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastPublish > publishInterval) {
    lastPublish = now;
    Serial.print("Mqtt status : ");
    Serial.println(Mqtt_status);

    get_data();
    SendData();

    if (client.connected()) {
      SendDataMQTT();
    }
  }

  // ตรวจสอบสถานะ Ethernet
  if (Ethernet.linkStatus() == 0) {
    Serial.println("Ethernet cable disconnected. Restarting...");
    ESP.restart();
  }
}

void get_data() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousLedMillis >= ledInterval) {
    previousLedMillis = currentMillis;

    ActRelay1 = digitalRead(RELAY1_PIN);
    ActRelay1 = !ActRelay1;
    ActFan1 = digitalRead(Ct1);
    ActFan1 = !ActFan1;

    ActRelay2 = digitalRead(RELAY2_PIN);
    ActRelay2 = !ActRelay2;
    ActFan2 = digitalRead(Ct2);
    ActFan2 = !ActFan2;

    if (ContRelay1 == 1) {
      if (ActRelay1 == 1) {
        AlarmFan1 = (ActFan1 == 1) ? 1 : 0;
      } else {
        AlarmFan1 = 0;
      }
    } else {
      AlarmFan1 = 1;
    }

    if (ContRelay2 == 1) {
      if (ActRelay2 == 1) {
        AlarmFan2 = (ActFan2 == 1) ? 1 : 0;
      } else {
        AlarmFan2 = 0;
      }
    } else {
      AlarmFan2 = 1;
    }

    digitalWrite(ALARM1_PIN, AlarmFan1);
    digitalWrite(ALARM2_PIN, AlarmFan2);

    ledState = !ledState;
    digitalWrite(ONBOARD_LED, ledState ? HIGH : LOW);
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    tempSensor.requestTemperatures();
    float newTemp = tempSensor.getTempCByIndex(0);
    if (newTemp >= 5) {
      temp = newTemp;
    }
    Serial.print("Temperature: ");
    Serial.println(temp);

    // ไม่ใช้ vTaskDelay ให้ loop ทำงานต่อเนื่อง
    SendData();
  }
}

void SendData() {
  char tempStr[10];
  dtostrf(temp, 4, 1, tempStr);

  char msgBuffer[30];
  snprintf(
    msgBuffer,
    sizeof(msgBuffer),
    "%s,%d,%d,%d,%d,%d,%d,%d,%d",
    tempStr,
    ContRelay1,
    ActRelay1,
    ActFan1,
    AlarmFan1,
    ContRelay2,
    ActRelay2,
    ActFan2,
    AlarmFan2);

  Serial.print("Sent Serial: ");
  Serial.println(msgBuffer);
}

void SendDataMQTT() {
  char tempStr[10];
  dtostrf(temp, 4, 1, tempStr);

  char msgBuffer[30];
  snprintf(
    msgBuffer,
    sizeof(msgBuffer),
    "%s,%d,%d,%d,%d,%d,%d,%d,%d",
    tempStr,
    ContRelay1,
    ActRelay1,
    ActFan1,
    AlarmFan1,
    ContRelay2,
    ActRelay2,
    ActFan2,
    AlarmFan2);

  client.publish(dataTopic, msgBuffer);
  Serial.print("Sent:BAACTW22/DATA ");
  Serial.println(msgBuffer);
}
