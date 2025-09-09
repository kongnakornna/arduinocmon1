

//#include <EthernetENC.h>          //ENC28j60

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <SPI.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <ETH.h>
#include <PCF8574.h>
#include "Arduino.h"
#include <Ethernet.h>           // เพิ่มบรรทัดนี้ที่บนสุดถ้ายังไม่มี
EthernetClient ethClient;
PubSubClient mqttClient(ethClient);

#define working_led  32

// String SN = "KEICR21072025V4203";  // Serial Number
String SN = "AIRCOM1-202509071949";  // Serial Number

#define EEPROM_SIZE 64   // กำหนดขนาด EEPROM ให้เหมาะสม (64 ไบต์ก็เพียงพอ)
int addressPeriod = 0; 
int addressWarning = 4; 


#define RXD2 15    // Rx2 EA-8
#define TXD2 16    // Tx2 EA-8

TaskHandle_t Task0;
//TaskHandle_t Task1;

//  DS18b20 temperature sensor  // <<
#define ONE_WIRE_BUS 33   
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
//  DS18b20 temperature sensor  // >>

//   PCF8574    //   <<
PCF8574 AlarmInput(0x26);   // input
PCF8574 RelayOutput(0x24);  // realy

#define AIR1                  0   // < Relay  1 ch
#define AIR2                  1
#define AlarmTemp             2
#define AlarmAir1             3
#define AlarmAir2             4
#define AlarmHumi             5
#define AlarmFire             6
#define AlarmErrorSensor      7  // < Relay  8 ch
int state_on = 0;
int state_off = 1;

#define Air1AlarmInput        0  // < Input 1 ch
#define Air2AlarmInput        1
#define HumiAlarmInput        2
#define FireAlarmInput        3
#define UPS1AlarmInput        4
#define UPS2AlarmInput        5
#define WaterleakAlarmInput   6
#define HSSDAlarmInput        7  // < Input 8 ch

int dataAir1AlarmInput =      1;
int dataAir2AlarmInput =      1;
int dataHumiAlarmInput =      1;
int dataFireAlarmInput =      1; 
int dataUPS1AlarmInput =      1;
int dataUPS2AlarmInput =      1;
int dataWaterleakAlarmInput = 1;
int dataHSSDAlarmInput =      1;


//   PCF8574    // >>


// กำหนดพินและ config Ethernet เดิม
// #define ETH_ADDR        0
#define ETH_POWER_PIN  -1
#define ETH_MDC_PIN    23
#define ETH_MDIO_PIN   18
#define ETH_TYPE       ETH_PHY_LAN8720
#define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT

// IP Address เดิม
// IPAddress local_ip(192, 168, 72, 5);
// IPAddress subnet(255, 255, 255, 0);
// IPAddress gateway(192, 168, 72, 254);
// IPAddress dns(8, 8, 8, 8);

IPAddress local_ip(172, 25, 99, 57);
IPAddress subnet(255, 255, 255, 240);
IPAddress gateway(172, 25, 99, 62);
IPAddress dns(8,8,8,8);  // เปลี่ยนชื่อเป็น dnsIP

// ****************** MQTT Settings **********************
// const char* dataTopic = "KEICR21072025V4203/DATA";                // ใช้สำหรับส่งข้อมูล MQTT
// const char* controlTopic = "KEICR21072025V4203/CONTROL";          // ใช้สำหรับสั่ง ปิด/เปิด


const char* dataTopic = "AIRCOM1/DATA";                // ใช้สำหรับส่งข้อมูล MQTT
const char* controlTopic = "AIRCOM1/CONTROL";          // ใช้สำหรับสั่ง ปิด/เปิด

const char* mqttServer = "172.25.99.60";     
// const char* mqttServer = "broker.hivemq.com";         // Cloud MQTT
//const char* mqttServer = "192.168.1.57";                // local MQTT
const int mqttPort = 1883;

// WiFiClient ethClient;
// PubSubClient mqttClient(ethClient);




int period = 6;   // 6 ชั่วโมง     จัดเก็บลงใน EEPROM
int Time1Hr = 3600000;  // 1 hr. = 3,600,000 s.    //  1 นาที = 60000

//   Interval   //

unsigned long Period_Interval_x = 0;      // เวลารอบล่าสุด (millis)
unsigned long TimeOffDelay_x = 0;         // เวลาเริ่มนับ delay (millis)
unsigned long nowMillis = 0;              // เวลา ณ ปัจจุบัน (millis)
//24 × 60 × 60 × 1000 = 86,400,000 ms   1 Hr. =3,600,000 ms
unsigned long Period_Interval = 21600000;   // 1 นาที = 60,000 ms  // 6 Hr. = 21600000
unsigned long TimeOffDelay = 600000;       // 10 วินาที = 10,000 ms   10 นาที มีค่าเท่ากับ 600,000 มิลลิวินาที (ms)

unsigned long lastUpdateLoop = 0;
unsigned long UpdateIntervalLoop = 5000; // 5 วินาที

unsigned long workingUpdate = 0;
bool status_working_led = true;
unsigned long workingInterval = 1000; // 1 วินาที

unsigned long readDataUpdate = 0;
unsigned long readDataInterval = 1000; // 1 วินาที

// --- สำหรับ MQTT กระพริบทุก 5 นาที ---
unsigned long lastPublishTime = 0;
const long publishInterval = 5000; // 5 วินาที

//   Interval   //

float currentTemp = 0.0;
int ERROR_temp = -127;  //-127
bool TempSensorConnect = true;
int status_temp_alarm = 1; // 0 = alarm 1 = normal   //น้ำหนัก  1
int TempAlarmOFF = 1;

int different_warning = 35;
int different_preiod = 6;
int TempRecovery = 32;   //  32 °C  TempRecovery = different_warning - TRecovery
int TRecovery = 3; 
int warning = 15; // warningTemp 35°C    จัดเก็บลงใน EEPROM
int warningTemp = 35;


int percent = 0;

int decimalValue;
char hexStr[3];

char firstChar;
String numberStr;
int numberPart = 0;

int MODE = 1; // 1= AUTO , 0 = MANUAL

bool state_AIR1 = false; // true = OFF (LOW), false = ON (HIGH)
bool state_AIR2 = true;
bool TimeSwitchOff = false;
int  lastOnRelay = 1; // 1 = relay1, 2 = relay2 , 3 = raley3

String Str_temp_alarm = ""; 
String Str_fire_alarm = "";
String Str_humi_alarm = "";
String Str_air1_alarm = "";
String Str_air2_alarm = "";
String Str_Code = "";
String cmdBuffer = "";

String cmd;
String cmd_different = "aaa";



void loop_T0(void * parameter) {
   for (;;) { 

         // รับค่าจาก HMI

     // รับค่าจาก HMI
  if (Serial2.available()) {
    cmd = Serial2.readStringUntil('\n');
    cmd.trim();
    Serial.print("handleHMICommand:");Serial.println(cmd);
    
    if(cmd_different != cmd){
      
                      if (cmd.length() >= 1) {
                          // แยกอักษรแรก
                          firstChar = cmd.charAt(0);

                           // แยกส่วนที่เหลือแล้วแปลงเป็นเลข
                          numberStr = cmd.substring(1);
                          numberPart = numberStr.toInt();  // แปลง String → int

                           // แสดงผล
                          Serial.print("First Character: ");
                          Serial.println(firstChar);
                          Serial.print("Number Part: ");
                          Serial.println(numberPart);
                     } else {}

              Check_Wording();
  
        }else{}
     cmd_different = cmd;   
    } 
       

   
     // รับค่าจาก HMI
  }
}


  

void Check_Wording(){

  //************  Schedule  **************
  if(firstChar=='s'){

             sendToHMI("main.t1.txt=\"Schedule\""); 
             MODE = 2; 
             sendToHMI("main.p5.x=374"); // x =383 y=101      x484  Schedule
             sendToHMI("main.p5.y=94"); // x =383 y=101      x484  Schedule
             sendToHMI("main.b1.x=485");  // 485
             sendToHMI("main.b2.x=485");  // 485
             sendToHMI("main.j0.val=0");
  
  }else{}  //  end if


  

  //************  Period  **************
  if(firstChar=='p'){
        //Serial.println("Check Period");
          if(numberPart==different_preiod){
                Serial.println("numberPart=different_Period");         
          }else{
                Serial.println("New data different_Period");
                different_preiod = numberPart;                        
                period = numberPart;
                Period_Interval = Time1Hr*period;  
                Save_data();

                String cmd = "main.cb1.val=" + String(period);
                sendToHMI(cmd.c_str());
            }          
  
  }else{}  //  end if
//************  warning  **************
  if(firstChar=='w'){
       //Serial.println("Check warning");
          if(numberPart==different_warning){
                Serial.println("numberPart=different_warning");         
          }else{
                Serial.println("New data different_warning");
                different_warning = numberPart;                    
                warning = numberPart-20; 
                warningTemp = numberPart;  
                TempRecovery = different_warning - TRecovery;   
                Save_data();

                String cmd = "main.cb0.val=" + String(warning);
                sendToHMI(cmd.c_str());
                              
            }       
    
  }else{}  

  //************  MODE  **************
  if(firstChar=='a'){          
   Serial.println("Mode : AUTO");
   MODE = 1; // 1= AUTO , 0 = MANUA 
   Control_mode(MODE);        

   sendToHMI("main.p5.x=482"); // x =383 y=101      x484  Schedule
  
   
  }else{}  //  end if

  if(firstChar=='m'){          
   Serial.println("Mode : MANUAL");
   MODE = 0; // 1= AUTO , 0 = MANUA 
   Control_mode(MODE);      
   sendToHMI("main.p5.x=482"); // x =383 y=101      x484  Schedule      
  }else{}  //  end if

  //************  Control Air 1  **************

    if(firstChar=='b'){
        //Serial.println("Control Air 1");
          if(numberPart==1){

                                  if(MODE == 0){
                                                      Serial.println("Control Air 1: STOP");   
                                                      state_AIR1 = true;  //STOP      
                                                      Control_relay(AIR1,state_off);                 
                                                      sendToHMI("main.t2.txt=\"STOP\""); 
                                        }else{}

                
          }else{}
          if(numberPart==2){

                                  if(MODE == 0){
                                                       Serial.println("Control Air 1: RUN");  
                                                       state_AIR1 = false;  //RUN   
                                                       Control_relay(AIR1,state_on); 
                                                       sendToHMI("main.t2.txt=\"RUN\"");     
                                        }else{}
                
          }else{}       
    
  }else{}  //  end if

   //************  Control Air 2  **************

    if(firstChar=='c'){
        //Serial.println("Control Air 2");
          if(numberPart==1){

                       
                                if(MODE == 0){
                                                      Serial.println("Control Air 2: STOP");   
                                                      state_AIR2 = true;  //STOP   
                                                      Control_relay(AIR2,state_off); 
                                                      sendToHMI("main.t3.txt=\"STOP\"");  
                                       }else{}

                
          }else{}
          if(numberPart==2){
            

                                if(MODE == 0){
                                                      Serial.println("Control Air 2: RUN");  
                                                      state_AIR2 = false;  //RUN   
                                                      Control_relay(AIR2,state_on); 
                                                      sendToHMI("main.t3.txt=\"RUN\"");     

                                       }else{}

                
          }else{}       
    
  }else{}  //  end if 
}


// ฟังก์ชัน callback สำหรับรับข้อความ MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  String message;
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    message += (char)payload[i];
  }

 //***********



    cmd = message;
    cmd.trim();
    Serial.print("Web Command:");Serial.println(cmd);
    
    if(cmd_different != cmd){
      
                      if (cmd.length() >= 1) {
                          // แยกอักษรแรก
                          firstChar = cmd.charAt(0);

                           // แยกส่วนที่เหลือแล้วแปลงเป็นเลข
                          numberStr = cmd.substring(1);
                          numberPart = numberStr.toInt();  // แปลง String → int

                           // แสดงผล
                          Serial.print("First Character: ");
                          Serial.println(firstChar);
                          Serial.print("Number Part: ");
                          Serial.println(numberPart);
                     } else {}

              Check_Wording();
  
        }else{}
     cmd_different = cmd; 

 //************
}

void Save_data(){
   EEPROM.write(addressPeriod, period);   
   EEPROM.commit(); 
   EEPROM.write(addressWarning, warning);   
   EEPROM.commit(); 
}

void setup(){    
  
    Serial.begin(9600);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    sensors.begin();   // begin sensor ds18b20
    Wire.begin(4, 5);  // setting pin PCF8574 SDA=21, SCL=22 (ค่าเริ่มต้นของ ESP32)
    AlarmInput.begin();
    RelayOutput.begin();

     state_AIR1 = false;       // Air1 On
     state_AIR2 = true;        // Air2 off
     TimeSwitchOff = false;

    //  Pin
      pinMode(working_led, OUTPUT);delay(10); digitalWrite(working_led,HIGH);

    // กำหนด output เริ่มต้น  
    RelayOutput.write(AIR1, LOW);                   // < Relay  1 ch
    RelayOutput.write(AIR2, HIGH);                  // < Relay  2 ch
    RelayOutput.write(AlarmTemp, HIGH);             // < Relay  3 ch
    RelayOutput.write(AlarmAir1, HIGH);             // < Relay  4 ch
    RelayOutput.write(AlarmAir2, HIGH);             // < Relay  5 ch
    RelayOutput.write(AlarmHumi, HIGH);             // < Relay  6 ch
    RelayOutput.write(AlarmFire, HIGH);             // < Relay  7 ch
    RelayOutput.write(AlarmErrorSensor, HIGH);      // < Relay  8 ch
  
  
        //กำหนดการใช้งาน 2 CPU
      xTaskCreatePinnedToCore(
      loop_T0, /* Function to implement the task */
      "Task0", /* Name of the task */
      5000, /* Stack size in words */
      NULL, /* Task input parameter */
      0, /* Priority of the task */
      &Task0, /* Task handle. */
      0); /* Core where the task should run */

  

//   Ethernet  //
   vTaskDelay(pdMS_TO_TICKS(1000));  // รอ 1 วินาที โดยไม่บล็อก core หลักอื่น ๆ
  ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
  if (ETH.config(local_ip, gateway, subnet, dns, dns) == false) {
    Serial.println("LAN8720 Configuration failed.");
  } else {
    Serial.println("LAN8720 Configuration success.");
  }
    Serial.print("IP Address: ");
    Serial.println(ETH.localIP());
    Serial.print("Subnet Mask: ");
    Serial.println(ETH.subnetMask());
    Serial.print("Gateway: ");
    Serial.println(ETH.gatewayIP());

  mqttClient.setServer("broker.hivemq.com", 1883);
  mqttClient.setCallback(mqttCallback);

 // ส่งค่าเริ่มต้นไป HMI  <<
        // หน่วงเวลาเพื่อรอให้ HMI พร้อม รับคำสั่ง   
        vTaskDelay(pdMS_TO_TICKS(3000));  // รอ 5 วินาที โดยไม่บล็อก core หลักอื่น ๆ
        sendToHMI("main.t1.txt=\"AUTO\"");
        sendToHMI("main.cb0.val=15");  //35
        sendToHMI("main.cb1.val=6");  // 6
        sendToHMI("main.t2.txt=\"RUN\"");
        sendToHMI("main.t3.txt=\"STOP\"");
        sendToHMI("main.p1.x=482"); // normal
        sendToHMI("main.p1.y=62"); // normal
        sendToHMI("main.t4.x=482"); // 482 /279
        sendToHMI("main.t4.y=115"); // 482 /279
        sendToHMI("main.j0.val=100");
        sendToHMI("main.p3.x=482"); // x =387 y=178
        sendToHMI("main.p3.y=178"); // x =387 y=178
        sendToHMI("main.tm2.en=1"); // ดาว กระพริบ AUTO MODE
        sendToHMI("main.tm3.en=1"); // ดาว กระพริบ AUTO MODE
        sendToHMI("main.tm4.en=1"); // ดาว กระพริบ AUTO MODE
        sendToHMI("main.tm5.en=1"); // ดาว กระพริบ AUTO MODE
        sendToHMI("main.tm6.en=1"); // ดาว กระพริบ AUTO MODE

        sendToHMI("main.p5.x=482"); // x =383 y=101      x484  Schedule
        
   // ส่งค่าเริ่มต้นไป HMI  >>
       state_AIR1 = false;  //RUN  
       Control_relay(AIR1,state_on);    // Start Air 1


}

void reconnect() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("KEICR21072025V4203")) {
      Serial.println("connected");
      // subscribe หัวข้อที่ต้องการ
      mqttClient.subscribe("KEICR21072025V4203/CONTROL");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 3 seconds");
      vTaskDelay(pdMS_TO_TICKS(2000));  // รอ 3 วินาที โดยไม่บล็อก core หลักอื่น ๆ
      
      loop_timer();Serial.println("Timer_couter loop reconnect");
    }
  }
}


void loop() {

   if (!mqttClient.connected()) {
    reconnect();
  }
  mqttClient.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastPublishTime > publishInterval) {
    lastPublishTime = currentMillis;

   SendDataMQTT();

     // working 1 วินาที
  if (millis() - workingUpdate> workingInterval) {
    workingUpdate = millis();
    status_working_led = !status_working_led;        
    digitalWrite(working_led,status_working_led); 
  }else{}

   
   }
   loop_timer();Serial.println("Timer_couter loop ");
 }   //   end loop



void loop_timer() {

    // working 1 วินาที
  if (millis() - readDataUpdate> readDataInterval) {
    readDataUpdate = millis();  
        Read_temperature();
        Read_AlarmInput();      
  }else{}

  
  // อ่านอุณหภูมิทุก 4 วินาที
  if (millis() - lastUpdateLoop > UpdateIntervalLoop) {
    lastUpdateLoop = millis();
    
    //****  Couter  *******
            if(MODE==1){
                      if(TempAlarmOFF == 0){
                                            if(currentTemp>TempRecovery){
                                                  
                                                    Control_relay(AIR1,state_on);
                                                    sendToHMI("main.t2.txt=\"RUN\"");
                                                   
                                                    Control_relay(AIR2,state_on); 
                                                    sendToHMI("main.t3.txt=\"RUN\"");
                                                    Serial.println("loop ON AIR1 and AIR2");
                                                                                 
                                            }else{
                                                    TempAlarmOFF = 1;
                                                    if(state_AIR1){
                                                              sendToHMI("main.t2.txt=\"STOP\"");
                                                               state_AIR1 = true;  //STOP  
                                                                Control_relay(AIR1,state_off);
                                                              }else{
                                                                    sendToHMI("main.t2.txt=\"RUN\"");
                                                                    state_AIR1 = false;  //RUN  
                                                                    Control_relay(AIR1,state_on);;                                                                    
                                                                   }

                                                  if(state_AIR2){
                                                                  sendToHMI("main.t3.txt=\"STOP\"");
                                                                  state_AIR2 = true;  //STOP  
                                                                  Control_relay(AIR2,state_off);
                                                              }else{
                                                                  sendToHMI("main.t3.txt=\"RUN\"");
                                                                  state_AIR2 = false;  //RUN  
                                                                  Control_relay(AIR2,state_on);                                                               
                                                                  }   
                                                                                                                  
                                                 }                                    
                                      }
                     else{Timer_couter(); }

                }else{
                  
                 // sendToHMI("main.t1.txt=\"MANUAL\"");
                  }                    
    
 // Print_report();
     
  }else{}


}   //   end loop Timer


void Timer_couter() {
  //Serial.println("Timer_couter loop");
  nowMillis = millis();

  unsigned long elapsed = nowMillis - Period_Interval_x;

  if (elapsed > Period_Interval) elapsed = Period_Interval;

  percent = (elapsed * 100.0) / Period_Interval;

  // เช็คครบเวลาช่วง period หรือยัง และยังไม่มี delayOff
  if (!TimeSwitchOff && elapsed >= Period_Interval) {
    if (!state_AIR1) {
      state_AIR2 = false;  // run
      Control_relay(AIR2,state_on); 
      Serial.println("AIR 2: RUN (Switch)");
      lastOnRelay = 2;
      sendToHMI("main.t3.txt=\"RUN\"");
    } else {
       state_AIR1 = false;  // run
       Control_relay(AIR1,state_on);      
      Serial.println("AIR 1: RUN (Switch)");
      lastOnRelay = 1;
      sendToHMI("main.t2.txt=\"RUN\"");
    }

    TimeSwitchOff = true;
    TimeOffDelay_x = nowMillis;   // เริ่มหน่วงปิดรีเลย์
    Period_Interval_x = nowMillis; // รีเซ็ตเวลานับใหม่
    percent = 0;
  }

  // หน่วงเวลาปิดรีเลย์เก่า
  if (TimeSwitchOff && (nowMillis - TimeOffDelay_x >= TimeOffDelay)) {
    if (lastOnRelay == 1) {
      state_AIR2 = true; // stop
      Control_relay(AIR2,state_off); 
      Serial.println("AIR 2: STOP (Delay complete)");
      sendToHMI("main.t3.txt=\"STOP\"");
    } else {
      state_AIR1 = true; // stop
      Control_relay(AIR1,state_off); 
      Serial.println("AIR 1: STOP (Delay complete)");
      sendToHMI("main.t2.txt=\"STOP\"");
    }
    TimeSwitchOff = false;
  }

  sendToHMI("main.j0.val=" + String(percent));

}

void Control_mode(int x) {// 0= MANUAL , 1=AUTO 
 if(x==0){ // MANUAL MODE
      Serial.println("MANUAL MODE");
      sendToHMI("main.t1.txt=\"MANUAL\"");
      sendToHMI("main.b1.x=135");  // 135
      sendToHMI("main.b2.x=227");  // 227      
      sendToHMI("main.tm2.en=0");
      sendToHMI("main.tm3.en=0");
      sendToHMI("main.tm4.en=0");
      sendToHMI("main.tm5.en=0");
      sendToHMI("main.tm6.en=0");  
      sendToHMI("main.j0.val=100");  
      
  
 }else{    // AUTO MODE
      Serial.println("AUTO MODE");
      state_AIR1 = false; //ON
      Control_relay(AIR1,state_on); 
      sendToHMI("main.t2.txt=\"RUN\"");
      state_AIR2 = true; //OFF
      Control_relay(AIR2,state_off);
      sendToHMI("main.t3.txt=\"STOP\"");
      
      sendToHMI("main.t1.txt=\"AUTO\"");
      sendToHMI("main.b1.x=485");  // 485
      sendToHMI("main.b2.x=485");  // 485

      sendToHMI("main.tm2.en=1");
      sendToHMI("main.tm3.en=1");
      sendToHMI("main.tm4.en=1");
      sendToHMI("main.tm5.en=1");
      sendToHMI("main.tm6.en=1");

      TimeSwitchOff = false;      
      TimeOffDelay_x = nowMillis;   // เริ่มหน่วงปิดรีเลย์
      Period_Interval_x = nowMillis; // รีเซ็ตเวลานับใหม่
      percent = 0;  
       
 }
}

void Control_relay(int nuber,int state){
  RelayOutput.write(nuber, state);  
}

void sendToHMI(String cmd) {     // ฟังก์ชันส่งข้อมูลไป HMI
  Serial2.print(cmd);
  Serial2.write(0xFF); Serial2.write(0xFF); Serial2.write(0xFF);
}

void Read_AlarmInput(void){     // อ่านค่าจาก digital input
 dataAir1AlarmInput = AlarmInput.read(Air1AlarmInput);
 dataAir2AlarmInput = AlarmInput.read(Air2AlarmInput);
 dataHumiAlarmInput = AlarmInput.read(HumiAlarmInput);
 dataFireAlarmInput = AlarmInput.read(FireAlarmInput); 
 dataUPS1AlarmInput = AlarmInput.read(UPS1AlarmInput);
 dataUPS2AlarmInput = AlarmInput.read(UPS2AlarmInput);
 dataWaterleakAlarmInput = AlarmInput.read(WaterleakAlarmInput);
 dataHSSDAlarmInput = AlarmInput.read(HSSDAlarmInput);

 Control_relay(AlarmTemp,status_temp_alarm);
 Control_relay(AlarmAir1,dataAir1AlarmInput);
 Control_relay(AlarmAir2,dataAir2AlarmInput);
 Control_relay(AlarmHumi,dataHumiAlarmInput);
 Control_relay(AlarmFire,dataFireAlarmInput);




Str_Code = "";
Str_temp_alarm =String(status_temp_alarm);
Str_fire_alarm =String(dataFireAlarmInput);
Str_humi_alarm =String(dataHumiAlarmInput);
Str_air1_alarm =String(dataAir1AlarmInput);
Str_air2_alarm =String(dataAir2AlarmInput);

  Str_Code += Str_fire_alarm;
  Str_Code += Str_humi_alarm;
  Str_Code += Str_air2_alarm;
  Str_Code += Str_air1_alarm;
  Str_Code += Str_temp_alarm;

  // Serial.println("Code: "+Str_Code);
  decimalValue = strtol(Str_Code.c_str(), NULL, 2);  // แปลงเป็นเลขฐาน 10
  sprintf(hexStr, "%02X", decimalValue);  
  sendToHMI("main.t4.txt=\"" + String(hexStr) + "\"");

    if(Str_Code == "11111"){
      sendToHMI("main.p1.x=482");
      sendToHMI("main.t4.x=482");
     // Serial.println("No Alarm");
    
  }else{
      sendToHMI("main.p1.x=290");
      sendToHMI("main.t4.x=290");
     // Serial.println("Alarm");

      Ck_Error_code(); //  Check and Action ERROR Code 
    }

}
void Ck_Error_code() {
      
  //*****
   if(status_temp_alarm == 0){
      Serial.println("High Temp");      
   }else{
      Serial.println("Temp normal");      
    }
   //***** 
   if(dataFireAlarmInput == 0){
      Serial.println("Fire Alarm");      
   }else{
      Serial.println("Fire normal");    
    }
   //***** 
      if(dataHumiAlarmInput == 0){
      Serial.println("Humi Alarm");      
   }else{
      Serial.println("Humi normal");    
    }
   //***** 
      if(dataAir1AlarmInput == 0){
      Serial.println("Air1 Alarm");      
   }else{
      Serial.println("Air1 normal");    
    }
   //*****
     if(dataAir2AlarmInput == 0){
      Serial.println("Air2 Alarm");      
   }else{
      Serial.println("Air2 normal");    
    }
}


void Read_temperature(){
    vTaskDelay(pdMS_TO_TICKS(2000));
    sensors.requestTemperatures();
    currentTemp = sensors.getTempCByIndex(0);
    if(currentTemp == ERROR_temp){        
        TempSensorConnect = false;
        currentTemp = 25 ;
        sendToHMI("main.p3.x=387"); // x =285 y=0
        Serial.println("Temp sensor error");  
        Control_relay(AlarmErrorSensor,state_on);
    }else{
           TempSensorConnect = true;
           sendToHMI("main.p3.x=482"); // x =285 y=0
           Control_relay(AlarmErrorSensor,state_off);
           }
    sendToHMI("main.t0.txt=\"" + String(currentTemp, 1) + "\"");
    Serial.print("Temperature Room : ");Serial.println(String(currentTemp)+"°C");
         if(currentTemp>different_warning){
      status_temp_alarm=0;
      TempAlarmOFF = 0;     
     }else{status_temp_alarm=1;}

     
}

void Print_report(){
  
 Serial.print("Temperature Room : ");Serial.println(String(currentTemp)+"°C");
 Serial.println("AlarmInput");
 Serial.print("[HSSD : ");Serial.print(dataHSSDAlarmInput);Serial.print("]");
 Serial.print("[Waterleak : ");Serial.print(dataWaterleakAlarmInput);Serial.print("]");
 Serial.print("[UPS2 : ");Serial.print(dataUPS2AlarmInput);Serial.print("]");
 Serial.print("[UPS1 : ");Serial.print(dataUPS1AlarmInput);Serial.print("]");
 Serial.print("[Fire : ");Serial.print(dataFireAlarmInput);Serial.print("]");
 Serial.print("[Humi : ");Serial.print(dataHumiAlarmInput);Serial.print("]");
 Serial.print("[Air2 : ");Serial.print(dataAir2AlarmInput);Serial.print("]");
 Serial.print("[Air1 : ");Serial.print(dataAir1AlarmInput);Serial.print("]");
 Serial.print("[Temp : ");Serial.print(status_temp_alarm);Serial.println("]");


 Serial.print("[Mode : ");Serial.print(MODE);Serial.print("]");
 Serial.print("[Period : ");Serial.print(period);Serial.print("]");
 Serial.print("[Period Interval : ");Serial.print(Period_Interval);Serial.print("]");
 Serial.print("[Warning Temp : ");Serial.print(warningTemp);Serial.print("]");
 Serial.print("[Temp Recovery : ");Serial.print(TempRecovery);Serial.println("]");

 Serial.print("[AIR 1 : ");Serial.print(state_AIR1);Serial.print("]");
 Serial.print("[AIR 2 : ");Serial.print(state_AIR2);Serial.println("]");
 
}


void SendDataMQTT() {
   // temperatrue
  char tempStr[10];
  dtostrf(currentTemp, 4, 1, tempStr);

  // warningTemp
  char warningTempStr[10];
  dtostrf(warningTemp, 4,1, warningTempStr);

  
  // TempRecovery
  char TempRecoveryStr[10];
  dtostrf(TempRecovery, 4,1, TempRecoveryStr);

  
  // period
  char periodStr[10];
  dtostrf(period, 4,1, periodStr);

  
  // percent
  char percentStr[10];
  dtostrf(percent, 4,1, percentStr);
  

  char msgBuffer[60];
  snprintf(
    msgBuffer,
    sizeof(msgBuffer),
    "%s,%s,%s,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
    tempStr,
    warningTempStr,
    TempRecoveryStr,
    periodStr,
    percentStr,
    dataFireAlarmInput,   
    dataHumiAlarmInput,
    dataAir2AlarmInput,
    dataAir1AlarmInput,
    status_temp_alarm,
    MODE,
    state_AIR1,
    state_AIR2,
    TempAlarmOFF,
    dataUPS1AlarmInput,
    dataUPS2AlarmInput,
    dataHSSDAlarmInput,
    dataWaterleakAlarmInput
    
  );
   Serial.println(msgBuffer);
   

  if (mqttClient.publish("KEICR21072025V4203/DATA", msgBuffer)) {
    Serial.println("Sent:KEICR21072025V4203/DATA ");    
  } else {
    Serial.println("Send failed!");
  }
}
