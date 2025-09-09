



#include <SPI.h>
#include <EthernetENC.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>


unsigned long previousEthernetCheck = 0;
const long ethernetCheckInterval = 5000;  // ตรวจสถานะทุก 5 วินาที

String SN = "BAACTW02";  // Serial Number


// ****************** Network Settings ******************
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};      // mac address ให้ใช้ตัวเดียวกันกับ esp32
IPAddress ip(192, 168, 1, 99);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 1, 1);
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

// ****************** MQTT Settings **********************
const char* dataTopic = "BAACTW02/DATA";                // ใช้สำหรับส่งข้อมูล MQTT
const char* controlTopic = "BAACTW02/CONTROL";          // ใช้สำหรับสั่ง ปิด/เปิด
//const char* mqttServer = "broker.hivemq.com";         // Cloud MQTT
const char* mqttServer = "192.168.1.57";                // local MQTT
const int mqttPort = 1883;

const int mVperAmp = 100;

float Analog1;
float Analog2;
float AnalogMin = 300;   //ค่าน้อยกว่า x = power off
float AnalogMax = 350;   //ค่ามากว่า x =  power on
float AnalogOver = 1000;  //ค่าสูงเกินกำหนด  Overload


int coun = 6;

float vpp1, vpp2;
float temp;
float tempBuffer;
int ContRelay1 = 0;
int ActRelay1 = 0;
int ActFan1 = 0;
int AlarmFan1 = 0;
int Fan1_Overload = 0;

int StatusActFan1 = 0;
int StatusAlarmFan1 = 0;
int StatusAlarmFan1OFF = 0;
int StatusAlarmFan1ON = 0;
int StatusFan1_Overload = 0;
int StatusFan1_OverloadOFF = 0;
int StatusFan1_OverloadON = 0;

int StatusFan1OFF = 0;
int StatusFan1ON = 0;

int ContRelay2 = 0;
int ActRelay2 = 0;
int ActFan2 = 0;
int AlarmFan2 = 0;
int Fan2_Overload = 0;

int StatusActFan2 = 0;
int StatusAlarmFan2 = 0;
int StatusAlarmFan2OFF = 0;
int StatusAlarmFan2ON = 0;
int StatusFan2_Overload = 0;

int StatusFan2_OverloadOFF = 0;
int StatusFan2_OverloadON = 0;

int StatusFan2OFF = 0;
int StatusFan2ON = 0;

int counter = 0;
int counter2 = 0;
/*
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

*/

// ****************** Pin Definitions *************Old*******
#define RELAY1_PIN   25
#define RELAY2_PIN   26
#define ALARM1_PIN   32
#define ALARM2_PIN   33
#define Analog1_PIN  34
#define Analog2_PIN  35
#define ONE_WIRE_BUS 15
#define ONBOARD_LED  2
#define POWER_LED    4

// ****************** Sensor Objects *********************
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// ****************** Variables **************************
unsigned long previousMillis = 0;
const long interval = 10000;

// --- สำหรับ LED กระพริบทุก 1 นาที ---
unsigned long previousLedMillis = 0;
const long ledInterval = 500; // 500 มิลลิวินาที
bool ledState = false;

// --- สำหรับ MQTT กระพริบทุก 5 นาที ---
unsigned long previousMQTTMillis = 0;
const long MQTTInterval = 5000; // 5 นาที
unsigned long previousMillisMQTT = 0;





void setup() {
  Serial.begin(9600);

  // Initialize Ethernet
  Ethernet.init(5);
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  delay(1500);
  //vTaskDelay(pdMS_TO_TICKS(1500)); 
  Serial.println("");
  Serial.print("IP: ");
  Serial.println(Ethernet.localIP());
  delay(1500);
  //vTaskDelay(pdMS_TO_TICKS(1500)); 

  // Initialize MQTT
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);

  // Initialize pin
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(ALARM1_PIN, OUTPUT);
  pinMode(ALARM2_PIN, OUTPUT);
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(POWER_LED, OUTPUT);
  delay(100);
  //vTaskDelay(pdMS_TO_TICKS(100)); 
  digitalWrite(RELAY1_PIN, HIGH);  //  <<<< OFF
  digitalWrite(RELAY2_PIN, LOW);  //  <<<< OFF
  digitalWrite(ALARM1_PIN, HIGH);
  digitalWrite(ALARM2_PIN, HIGH);
  digitalWrite(ONBOARD_LED, HIGH);
  digitalWrite(POWER_LED, HIGH);
  delay(100);
  // vTaskDelay(pdMS_TO_TICKS(100)); 
  
       
 // Initialize Temperature Sensor
  tempSensor.begin();
  Serial.println("System Ready");
}


void callback(char* topic, byte* payload, unsigned int length) {
  char msg[length+1];
  memcpy(msg, payload, length);
  msg[length] = '\0';

  String str = String(msg);
  Serial.print("Received: "); Serial.println(str);

  if(str=="0"){
    ContRelay1 = 0;
    digitalWrite(RELAY1_PIN, HIGH);
    Serial.println("FAN1 : OFF");
  }
  if(str=="1"){
    ContRelay1 = 1;
    digitalWrite(RELAY1_PIN, LOW);
    Serial.println("FAN1 : ON");
  }
  if(str=="2"){
    ContRelay2 = 0;
    digitalWrite(RELAY2_PIN, HIGH);
    Serial.println("FAN2 : OFF");
  }
  if(str=="3"){
    ContRelay2 = 1;
    digitalWrite(RELAY2_PIN, LOW);
    Serial.println("FAN2 : ON");
  }
}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting MQTT...");
    if (mqttClient.connect("BAACTW02")) {
      mqttClient.subscribe(controlTopic);
      Serial.println("OK");
      digitalWrite(POWER_LED, LOW);
    } else {
      digitalWrite(POWER_LED, HIGH);
      Serial.println("Retrying...");
      delay(5000);
      //vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
  }
}


void checkEthernetConnection() {
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable disconnected. Trying to reconnect...");
        // ปิดการเชื่อมต่อเดิม
        Ethernet.end();
        delay(1000);
        //vTaskDelay(pdMS_TO_TICKS(1000)); // รอเล็กน้อยก่อนเชื่อมต่อใหม่
        // เริ่มเชื่อมต่อใหม่
        Ethernet.begin(mac, ip, gateway, gateway, subnet);
        delay(1500);
       // vTaskDelay(pdMS_TO_TICKS(1500));  // รอให้เชื่อมต่อเสร็จ
        Serial.print("New IP: ");
        Serial.println(Ethernet.localIP());
    }
}



void loop() {

  getVPPBoth(&vpp1, &vpp2);
  Check_statusFan1(vpp1);
  Check_statusFan2(vpp2);
  
}

void Check_statusFan1(float peakToPeakVoltage) {
  delay(100);
  //ActRelay1 = digitalRead(Analog1_PIN);  
  double VRMS = (peakToPeakVoltage / 2.0) * 0.707;
  double AmpsRMS = (VRMS * 800) / mVperAmp;
 
  Serial.print("Analog 1");
  Serial.print(" :");
  Serial.print(AmpsRMS);
  Serial.print(" : ");
  
  if(AnalogMin > AmpsRMS){
   // Serial.println("  Fan 1 status : OFF");
    StatusActFan1 = 0;
    StatusFan1_Overload = 0;
  }
  if(AmpsRMS > AnalogOver){
   // Serial.println("  Fan 1 status : Over load");
    StatusActFan1 = 1;
    StatusFan1_Overload = 1;
  } else {
    if(AmpsRMS > AnalogMax){
    //  Serial.println("  Fan 1 status : ON");
      StatusFan1_Overload = 0;
      StatusActFan1 = 1;
    }
  }

}

void Check_statusFan2(float peakToPeakVoltage) {
  delay(100);
  //ActRelay1 = digitalRead(Analog1_PIN);  
  double VRMS = (peakToPeakVoltage / 2.0) * 0.707;
  double AmpsRMS = (VRMS * 800) / mVperAmp;
 
  Serial.print("Analog 2");
  Serial.print(" :");
  Serial.print(AmpsRMS);
  Serial.println(" : ");

  if(AnalogMin > AmpsRMS){
  //  Serial.println("  Fan 2 status : OFF");
    StatusActFan2 = 0;
    StatusFan2_Overload = 0;
  }
  if(AmpsRMS > AnalogOver){
  //  Serial.println("  Fan 2 status : Over load");
    StatusActFan2 = 1;
    StatusFan2_Overload = 1;
  } else {
    if(AmpsRMS > AnalogMax){
  //    Serial.println("  Fan 2 status : ON");
      StatusFan2_Overload = 0;
      StatusActFan2 = 1;
    }
  }

}
void getVPPBoth(float* vpp1, float* vpp2) {
  int maxValue1 = 0, minValue1 = 4096;
  int maxValue2 = 0, minValue2 = 4096;
  uint32_t start_time = millis();

  while ((millis() - start_time) < 500) {
    int readValue1 = analogRead(Analog1_PIN);
    int readValue2 = analogRead(Analog2_PIN);

    if (readValue1 > maxValue1) maxValue1 = readValue1;
    if (readValue1 < minValue1) minValue1 = readValue1;

    if (readValue2 > maxValue2) maxValue2 = readValue2;
    if (readValue2 < minValue2) minValue2 = readValue2;
  }

  *vpp1 = ((maxValue1 - minValue1) * 3.3) / (4096.0 / 1000);
  *vpp2 = ((maxValue2 - minValue2) * 3.3) / (4096.0 / 1000);
}

void SendData() {
  delay(100);
  //vTaskDelay(pdMS_TO_TICKS(100)); 

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
    AlarmFan2
  );

    Serial.print("Sent Serial: ");
    Serial.println(msgBuffer);

}

void SendDataMQTT() {
  delay(100);
  //vTaskDelay(pdMS_TO_TICKS(100));

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
    AlarmFan2
  );
  if (mqttClient.publish("BAACTW02/DATA", msgBuffer)) {
    Serial.print("Sent:BAACTW02/DATA ");
    Serial.println(msgBuffer);
  } else {
    Serial.println("Send failed!");
  }
}