#include <EthernetENC.h>
#include <PubSubClient.h>
#include <OneWire.h>
OneWire ds(0);  // on pin 10 (a 4.7K resistor is necessary) D3
#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>

String SN = "KEIPW11082025V2001";  // Serial Number

int pinValue = 0;

#include <PZEM004Tv30.h>
PZEM004Tv30 pzem(4, 5);  // กำหนด ขา ที่เชื่อมต่อกับ PZEM004  V3.0

#define EEPROM_SIZE 512
#define blynk_con    2    // D4
long currentMillis=0;
float volt = 220;
float amp = 0;
float powerX = 0;
float freq = 50;
float oldEnergy;
float EnergyALL;
float energyX;
float pf;
float celsius;
int  address_energy=0;

#define RelayXX     13         //D7    Relay control ON/OFF

#define DHTTYPE DHT21   // DHT 21 (AM2301)

const char* mqtt_server = "broker.hivemq.com"; // Cloud MQTT

byte mac[] = {0xA4, 0xCF, 0x12, 0xDD, 0x5F, 0xCA}; // MAC address ของ Ethernet module
IPAddress ip(192, 168, 1, 20);
IPAddress myDns(8, 8, 8, 8);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

EthernetClient ethClient;
PubSubClient client(ethClient);

long now = millis();
long lastMeasure = 0;
String data1;

void setup_ethernet() {
  Serial.println();
  Serial.println("Ethernet setup...");
  Ethernet.begin(mac, ip, myDns, gateway, subnet);
  delay(1000);
  Serial.print("Ethernet connected - IP address: ");
  Serial.println(Ethernet.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();
  if (String(topic) == "control/RelayXX") {
    Serial.print("control RelayXX ");
    data1 = messageTemp;
  }
  Serial.print("data = ");
  Serial.println(data1);
  if (data1 == "OFF") {
    digitalWrite(RelayXX, HIGH);
    delay(100);
  }
  if (data1 == "ON") {
    digitalWrite(RelayXX, LOW);
    delay(100);
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("KEIPW11082025V2001")) {
      Serial.println("connected");
      client.subscribe("KEIPW11082025V2001/CONTROL");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  pinMode(RelayXX, OUTPUT);

  delay(500);
  digitalWrite(RelayXX, HIGH);
  delay(500);

  Serial.begin(9600);

  setup_ethernet();

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  now = millis();
  if (now - lastMeasure > 3500) {
    lastMeasure = now;
    readDataPDU();
   // readTemp();
    sendDataMqtt();
  }
}

// ส่วน readDataPDU, readTemp, sendDataMqtt ใช้ของเดิมได้เลย ไม่ต้องเปลี่ยน

void readDataPDU() {
    
    float voltage = pzem.voltage();
    volt = voltage;
    if(voltage != NAN){
        Serial.print("Voltage: "); Serial.print(voltage); Serial.println("V");
       
    } else {
        Serial.println("Error reading voltage");
    }

    float current = pzem.current();
    amp = current;
    if(current != NAN){
        Serial.print("Current: "); Serial.print(current); Serial.println("A");
        
    } else {
        Serial.println("Error reading current");
    }

     powerX = pzem.power();
    if(current != NAN){
        Serial.print("Power: "); Serial.print(powerX); Serial.println("W");

    } else {
        Serial.println("Error reading power");
    }

        energyX = pzem.energy();
        if(current != NAN){
        oldEnergy = EEPROM.read(address_energy);
        EnergyALL = oldEnergy+energyX;
        Serial.print("Energy: "); Serial.print(EnergyALL,3); Serial.println("kWh");
        EEPROM.write(address_energy,EnergyALL);
        EEPROM.commit();
       
    } else {
        Serial.println("Error reading energy");
    }

    float frequency = pzem.frequency();
        freq = frequency;
    if(current != NAN){
        Serial.print("Frequency: "); Serial.print(frequency, 1); Serial.println("Hz");
       
    } else {
        Serial.println("Error reading frequency");
    }

        pf = pzem.pf();
    if(current != NAN){
        Serial.print("PF: "); Serial.println(pf);
       
    } else {
        Serial.println("Error reading power factor");
    }

    Serial.println();

    
   }

void readTemp() {
  byte i;
  byte present = 0;
  byte type_s;
  byte data[12];
  byte addr[8];
  if ( !ds.search(addr)) {
    ds.reset_search();
    delay(250);
    return;
  }
  if (OneWire::crc8(addr, 7) != addr[7]) {
      return;
  }
  switch (addr[0]) {
    case 0x10:
      type_s = 1;
      break;
    case 0x28:
      type_s = 0;
      break;
    case 0x22:
      type_s = 0;
      break;
    default:
      return;
  } 
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  delay(1000);     // maybe 750ms is enough, maybe not
 
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
    raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
   
  }
  celsius = (float)raw / 16.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.println(" Celsius, ");
  

  
}
void sendDataMqtt() {

  char Temp_Mqtt[7];
  dtostrf(celsius, 6, 2, Temp_Mqtt);
  
   char Volt_Mqtt[7];
  dtostrf(volt, 6, 2, Volt_Mqtt);
        
   char amp_Mqtt[7];
  dtostrf(amp, 6, 2, amp_Mqtt);
        
   char power_Mqtt[7];
  dtostrf(powerX, 6, 2, power_Mqtt);
     
   char energy_Mqtt[7];
  dtostrf(energyX, 6, 2, energy_Mqtt);
         
  char freq_Mqtt[7];
  dtostrf(freq, 6, 2, freq_Mqtt);

   char pf_Mqtt[7];
  dtostrf(pf, 6, 2, pf_Mqtt);


  char msgBuffer[60];
  snprintf(
    msgBuffer,
    sizeof(msgBuffer),
    "%s,%s,%s,%s,%s,%s,%s",
    Temp_Mqtt,
    Volt_Mqtt,
    amp_Mqtt,
    power_Mqtt,
    energy_Mqtt,
    freq_Mqtt,
    pf_Mqtt
    
  );
   Serial.println(msgBuffer);



  if (client.publish("KEIPW11082025V2001/DATA", msgBuffer)) {
    Serial.println("Sent:KEIPW11082025V2001/DATA ");    
  } else {
    Serial.println("Send failed!");
  }
  
}
