#include <SPI.h>
#include <EthernetENC.h>

byte mac[] = {0xA4, 0xCF, 0x12, 0xDD, 0x5F, 0xCA}; // MAC address ของ Ethernet module
IPAddress ip(192, 168, 1, 20);
IPAddress myDns(8, 8, 8, 8);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() {
  Serial.begin(9600);
  delay(1000);
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  Serial.println("ENC28J60 Test");
  Serial.print("IP address: ");
  Serial.println(Ethernet.localIP());
  Serial.print("Subnet    : ");
  Serial.println(Ethernet.subnetMask());
  Serial.print("Gateway   : ");
  Serial.println(Ethernet.gatewayIP());
  Serial.print("DNS       : ");
  Serial.println(Ethernet.dnsServerIP());
}

void loop() {
  Serial.println("ENC28J60 Test");
  delay(3000);
}
