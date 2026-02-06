#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <esp_task_wdt.h>    // สำหรับ Watchdog ESP32
#include <Preferences.h>     // สำหรับเก็บข้อมูลถาวร

// กำหนดค่า Network
byte mac[] = { 0x80, 0xF3, 0xDA, 0x4B, 0x21, 0x4C };
// IPAddress ip(192, 168, 72, 6);
// IPAddress gateway(192, 168, 72, 1);
// IPAddress subnet(255, 255, 255, 0);
// IPAddress dnsIP(8, 8, 8, 8);

 
IPAddress ip(172, 25, 99, 59);
IPAddress subnet(255, 255, 255, 240);
IPAddress gateway(172, 25, 99, 62);
IPAddress dnsIP(8, 8, 8, 8);


// MQTT broker
// ****************** MQTT Settings **********************
const char* dataTopic= "BAACTW02/DATA";                // ใช้สำหรับส่งข้อมูล MQTT
const char* controlTopic = "BAACTW02/CONTROL";          // ใช้สำหรับสั่ง ปิด/เปิด
//const char* mqttServer = "broker.hivemq.com";         // Cloud MQTT
const char* mqttServer = "172.25.99.60";                // local MQTT
const int mqttPort = 1883;

 
EthernetClient ethClient;
PubSubClient client(ethClient);

Preferences preferences;

float Analog1;
float Analog2;
float AnalogMin = 300;   //ค่าน้อยกว่า x = power off
float AnalogMax = 350;   //ค่ามากกว่า x =  power on
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

// ****************** Pin Definitions ***************
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

// ****************** Sensor Objects *****************
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// ****************** Variables **********************
unsigned long previousMillis = 0;
const long interval = 10000;       // 10 วินาที

// LED กระพริบทุก 0.5 วินาที
unsigned long previousLedMillis = 0;
const long ledInterval = 500;
bool ledState = false;

unsigned long lastPublish = 0;
const unsigned long publishInterval = 3000;  // 3 วินาที

// ฟังก์ชัน Callbacks MQTT message
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
      preferences.putInt("ContRelay1", ContRelay1);
    } else if (message == "1") {
      ContRelay1 = 1;
      digitalWrite(RELAY1_PIN, LOW);
      Serial.println("FAN1 : ON");
      preferences.putInt("ContRelay1", ContRelay1);
    } else if (message == "2") {
      ContRelay2 = 0;
      digitalWrite(RELAY2_PIN, HIGH);
      Serial.println("FAN2 : OFF");
      preferences.putInt("ContRelay2", ContRelay2);
    } else if (message == "3") {
      ContRelay2 = 1;
      digitalWrite(RELAY2_PIN, LOW);
      Serial.println("FAN2 : ON");
      preferences.putInt("ContRelay2", ContRelay2);
    }
  }
}

// เชื่อมต่อ MQTT พร้อม exponential backoff
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
      esp_task_wdt_reset();  // feed watchdog
      break;
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      retryCount++;
      int delayTime = 3000 * retryCount; // exponential backoff
      if (delayTime > 30000) delayTime = 30000;  // max 30 วินาที
      Serial.print(" retrying in ");
      Serial.print(delayTime);
      Serial.println(" ms");
      Mqtt_status = 0;
      digitalWrite(POWER_LED, HIGH);
      vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
    esp_task_wdt_reset();  // feed watchdog
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

  // เริ่ม Preferences
  preferences.begin("relay", false);

  // โหลดสถานะรีเลย์ก่อนหน้านี้
  ContRelay1 = preferences.getInt("ContRelay1", 0);
  ContRelay2 = preferences.getInt("ContRelay2", 0);

  // ตั้งรีเลย์ตามสถานะที่โหลดมา
  digitalWrite(RELAY1_PIN, ContRelay1 == 1 ? LOW : HIGH);
  digitalWrite(RELAY2_PIN, ContRelay2 == 1 ? LOW : HIGH);

  Serial.println("System Ready");

  esp_task_wdt_init(5, true);   // 10 วินาที timeout, auto reset watchdog
  esp_task_wdt_add(NULL);         // add current task to watchdog
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

  // ตรวจสอบสถานะ Ethernet หากสายหลุด ให้รีสตาร์ท
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
  esp_task_wdt_reset();  // feed watchdog
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
  Serial.print("Sent:BAACTW02/DATA ");
  Serial.println(msgBuffer);
  esp_task_wdt_reset();  // feed watchdog
}
