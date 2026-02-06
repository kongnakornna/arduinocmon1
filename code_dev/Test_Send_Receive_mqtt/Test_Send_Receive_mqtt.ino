#include <UIPEthernet.h>
#include <PubSubClient.h>

// กำหนดค่า Network
byte mac[] = { 0xCC, 0xBD, 0xA7, 0x99, 0x87, 0x70 };
IPAddress ip(192,168,72,6);
IPAddress gateway(192,168,72,254);
IPAddress subnet(255,255,255,0);
IPAddress dnsIP(8,8,8,8);  // เปลี่ยนชื่อเป็น dnsIP

// MQTT broker
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;

const char* dataTopic = "BAACTW22/DATA";
const char* controlTopic = "BAACTW22/CONTROL";

EthernetClient ethClient;
PubSubClient client(ethClient);

volatile int state = 0;
unsigned long lastPublish = 0;
const unsigned long publishInterval = 3000; // 3 วินาที

void messageReceived(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == controlTopic) {
    if (message == "0") {
      Serial.println("Ralay Off");
      state = 0;
    } 
    else if (message == "1") {
      Serial.println("Ralay On");
      state = 1;
    }
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
  Serial.begin(9600);

  Ethernet.begin(mac, ip, dnsIP, gateway, subnet);
  delay(1500);

  client.setServer(mqttServer, mqttPort);
  client.setCallback(messageReceived);

  randomSeed(micros());

  Serial.println("Setup done");
}

void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastPublish > publishInterval) {
    lastPublish = now;

    String message = "Hi Alex " + String(state);
    if (client.connected()) {
      client.publish(dataTopic, message.c_str());
    }

    Serial.println(message);  // พิมพ์ออกทาง Serial ทุก 3 วินาที
  }
}
