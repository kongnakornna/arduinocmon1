#include <UIPEthernet.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// กำหนดค่า Network
byte mac[] = { 0xCC, 0xBD, 0xA7, 0x99, 0x87, 0x70 };
// IPAddress ip(192,168,72,6);
// IPAddress gateway(192,168,72,254);
// IPAddress subnet(255,255,255,0);
// IPAddress dnsIP(8,8,8,8);  // เปลี่ยนชื่อเป็น dnsIP
IPAddress ip(192,168,1,59);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
IPAddress dnsIP(8,8,8,8);  // เปลี่ยนชื่อเป็น dnsIP
// MQTT broker
// const char* mqttServer = "broker.hivemq.com";
const char* mqttServer = "192.168.1.100";
const int mqttPort = 1883;

const char* dataTopic = "BAACTW02/DATA";
const char* controlTopic = "BAACTW02/CONTROL";

EthernetClient ethClient;
PubSubClient client(ethClient);

const int mVperAmp = 100;

float Analog1;
float Analog2;
float AnalogMin = 300;   //ค่าน้อยกว่า x = power off
float AnalogMax = 350;   //ค่ามากว่า x =  power on
float AnalogOver1 = 1000;  //ค่าสูงเกินกำหนด  Overload
float AnalogOver2 = 2500;  //ค่าสูงเกินกำหนด  Overload


TaskHandle_t Task0;


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

// --- สำหรับ LED กระพริบทุก 1 นาที ---
unsigned long previousLedMillis = 0;
const long ledInterval = 500; // 500 มิลลิวินาที
bool ledState = false;

// --- สำหรับ MQTT กระพริบทุก 5 นาที ---
unsigned long previousMQTTMillis = 0;
const long MQTTInterval = 5000; // 5 นาที
unsigned long previousMillisMQTT = 0;

volatile int state = 0;
unsigned long lastPublish = 0;
const unsigned long publishInterval = 3000; // 3 วินาที

void messageReceived(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == controlTopic) {
    
                                   if(message=="0"){
                                    ContRelay1 = 0;
                                    digitalWrite(RELAY1_PIN, HIGH);
                                    Serial.println("FAN1 : OFF");
                                    }else{}
                                   if(message=="1"){
                                    ContRelay1 = 1;
                                    digitalWrite(RELAY1_PIN, LOW);
                                    Serial.println("FAN1 : ON");
                                    }else{}
                                
                                   if(message=="2"){
                                    ContRelay2 = 0;
                                    digitalWrite(RELAY2_PIN, HIGH);
                                    Serial.println("FAN2 : OFF");
                                    }else{}
                                   if(message=="3"){
                                    ContRelay2 = 1;
                                    digitalWrite(RELAY2_PIN, LOW);
                                    Serial.println("FAN2 : ON");
                                    }else{}
                                    
                                      }
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");

      client.subscribe(controlTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {

  // Initialize pin
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(ALARM1_PIN, OUTPUT);
  pinMode(ALARM2_PIN, OUTPUT);
  pinMode(ONBOARD_LED, OUTPUT);
  pinMode(POWER_LED, OUTPUT);

  pinMode(Ct1, INPUT);
  pinMode(Ct2, INPUT);

  
  vTaskDelay(pdMS_TO_TICKS(100)); 
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

  randomSeed(micros());


  vTaskDelay(pdMS_TO_TICKS(100)); 
      
 // Initialize Temperature Sensor
  tempSensor.begin();
  Serial.println("System Ready");
  
}

void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastPublish > publishInterval) {
    lastPublish = now;


    get_data();
    SendData();
    
 
    if (client.connected()) {


    
      SendDataMQTT(); 

      
    }
  }
}

void get_data(){
    unsigned long currentMillis = millis();

  // --- กระพริบ ONBOARD_LED ทุก 1 นาที ---
  if (currentMillis - previousLedMillis >= ledInterval) {  
    previousLedMillis = currentMillis;


    //Serial.print("ContRelay1 : "); Serial.println(ContRelay1);
    ActRelay1 = digitalRead(RELAY1_PIN);
    ActRelay1 = !ActRelay1;
   // Serial.print("ActRelay1 : "); Serial.println(ActRelay1);
    ActFan1 = digitalRead(Ct1);
    ActFan1 =!ActFan1;
   // Serial.print("ActFan1 : "); Serial.println(ActFan1);

    //Serial.print("ContRelay2 : "); Serial.println(ContRelay2);
    ActRelay2 = digitalRead(RELAY2_PIN);
    ActRelay2 = !ActRelay2;
    //Serial.print("ActRelay2 : "); Serial.println(ActRelay2);
    ActFan2 = digitalRead(Ct2);
    ActFan2 =!ActFan2;
    //Serial.print("ActFan2 : "); Serial.println(ActFan2);

    
  if(ContRelay1 == 1){
    if(ActRelay1 == 1){
      if(ActFan1 == 1) {
             AlarmFan1 = 1;
      } else { AlarmFan1 = 0;}
    } else { AlarmFan1 = 0;}
  } else { AlarmFan1 = 1;}

  if(ContRelay2 == 1){
    if(ActRelay2 == 1){
      if(ActFan2 == 1) {
        AlarmFan2 = 1;
      } else { AlarmFan2 = 0;}
    } else { AlarmFan2 = 0;}
  } else { AlarmFan2 = 1;}




    digitalWrite(ALARM1_PIN, AlarmFan1);
    digitalWrite(ALARM2_PIN, AlarmFan2);
   
    ledState = !ledState;
    digitalWrite(ONBOARD_LED, ledState ? HIGH : LOW);    
  }

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    tempSensor.requestTemperatures();
    temp = tempSensor.getTempCByIndex(0);

    tempBuffer = temp;
    if(temp < 5){
           temp = tempBuffer;
    }else{
           tempBuffer = temp;      
      }
    
    Serial.print("Temperature: "); Serial.println(temp);
    vTaskDelay(pdMS_TO_TICKS(1000)); 
    
     SendData();
  }
  
}

void SendData() {
  vTaskDelay(pdMS_TO_TICKS(100)); 

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
  vTaskDelay(pdMS_TO_TICKS(100));

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
      client.publish(dataTopic, msgBuffer);
      Serial.print("Sent:BAACTW02/DATA ");
      Serial.println(msgBuffer);
}
