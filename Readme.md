พบปัญหาการเชื่อมต่อ MQTT (rc=-2) ซึ่งหมายถึงการเชื่อมต่อล้มเหลว อาจเกิดจากหลายสาเหตุ มาวิเคราะห์และแก้ไขทีละจุด:

## ปัญหาและวิธีแก้ไข:

### 1. **ตรวจสอบการเชื่อมต่อ Ethernet**
```cpp
// ใน loop() เพิ่มการตรวจสอบสถานะ Ethernet
if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable disconnected!");
    digitalWrite(POWER_LED, HIGH); // ไฟสว่างเมื่อสายหลุด
} else if (Ethernet.linkStatus() == LinkON) {
    digitalWrite(POWER_LED, LOW); // ไฟดับเมื่อเชื่อมต่อแล้ว
}
```

### 2. **เพิ่ม Debug Ethernet**
```cpp
void printNetworkStatus() {
    Serial.print("Ethernet IP: ");
    Serial.println(Ethernet.localIP());
    Serial.print("Gateway: ");
    Serial.println(Ethernet.gatewayIP());
    Serial.print("DNS: ");
    Serial.println(Ethernet.dnsServerIP());
    Serial.print("Link Status: ");
    Serial.println(Ethernet.linkStatus() == LinkON ? "Connected" : "Disconnected");
}
```

### 3. **แก้ไขฟังก์ชัน connectMQTT()**
```cpp
void connectMQTT() {
    if (Ethernet.linkStatus() != LinkON) {
        Serial.println("Ethernet not connected. Waiting...");
        return;
    }
    
    printNetworkStatus(); // แสดงสถานะเครือข่าย
    
    int retryCount = 0;
    int maxRetries = 5;
    
    while (!client.connected() && retryCount < maxRetries) {
        Serial.print("Attempting MQTT connection to ");
        Serial.print(mqttServer);
        Serial.print(":");
        Serial.print(mqttPort);
        Serial.print("...");
        
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("connected!");
            Mqtt_status = 1;
            client.subscribe(controlTopic);
            digitalWrite(POWER_LED, LOW);
            retryCount = 0;
            esp_task_wdt_reset();
            return;
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.print(" error: ");
            switch(client.state()) {
                case -4: Serial.println("connection timeout"); break;
                case -3: Serial.println("connection lost"); break;
                case -2: Serial.println("connect failed"); break;
                case -1: Serial.println("client disconnected"); break;
                case 1: Serial.println("bad protocol"); break;
                case 2: Serial.println("bad client id"); break;
                case 3: Serial.println("unavailable"); break;
                case 4: Serial.println("bad credentials"); break;
                case 5: Serial.println("unauthorized"); break;
                default: Serial.println("unknown error");
            }
            
            retryCount++;
            int delayTime = min(3000 * retryCount, 10000); // max 10 seconds
            Serial.print("Retrying in ");
            Serial.print(delayTime);
            Serial.println(" ms");
            
            Mqtt_status = 0;
            digitalWrite(POWER_LED, HIGH);
            delay(delayTime);
        }
        esp_task_wdt_reset();
    }
    
    if (retryCount >= maxRetries) {
        Serial.println("Max MQTT connection retries reached. Will try again later.");
    }
}
```

### 4. **เพิ่มฟังก์ชันทดสอบ Ping**
```cpp
bool testNetworkConnection() {
    // ทดสอบ ping ไปยัง gateway
    IPAddress testIP = gateway;
    
    EthernetClient pingClient;
    if (pingClient.connect(testIP, 80)) {
        pingClient.stop();
        Serial.println("Network connectivity OK");
        return true;
    } else {
        Serial.println("Network connectivity FAILED");
        return false;
    }
}
```

### 5. **แก้ไขใน setup()**
```cpp
void setup() {
    // ... รหัสเดิม ...
    
    Serial.begin(115200); // เพิ่มความเร็วเป็น 115200
    Serial.println("\n\nSystem Starting...");
    
    // เริ่มต้น Ethernet
    Ethernet.begin(mac, ip, dnsIP, gateway, subnet);
    delay(2000); // รอการ initial Ethernet
    
    printNetworkStatus(); // แสดงสถานะเครือข่าย
    
    // ทดสอบการเชื่อมต่อ network
    if (testNetworkConnection()) {
        Serial.println("Network test passed");
    } else {
        Serial.println("Network test failed");
    }
    
    // ... รหัสเดิม ...
}
```

### 6. **ตรวจสอบ MQTT Broker**
```cpp
// ลองใช้ MQTT broker อื่นเพื่อทดสอบ
// const char* mqttServer = "test.mosquitto.org";
// const int mqttPort = 1883;
```

### 7. **เพิ่ม Watchdog สำหรับการจัดการปัญหา**
```cpp
// ใน loop() เพิ่ม
static unsigned long lastConnectionCheck = 0;
const unsigned long connectionCheckInterval = 30000; // ทุก 30 วินาที

if (millis() - lastConnectionCheck > connectionCheckInterval) {
    lastConnectionCheck = millis();
    
    if (!client.connected()) {
        Serial.println("MQTT disconnected. Reconnecting...");
        connectMQTT();
    }
    
    // รีบู๊ตถ้าไม่สามารถเชื่อมต่อได้เป็นเวลานาน
    static unsigned long lastConnectedTime = 0;
    if (client.connected()) {
        lastConnectedTime = millis();
    } else if (millis() - lastConnectedTime > 300000) { // 5 นาที
        Serial.println("No MQTT connection for 5 minutes. Restarting...");
        ESP.restart();
    }
}
```

### 8. **แก้ไขฟังก์ชัน loop()**
```cpp
void loop() {
    esp_task_wdt_reset();
    
    // ตรวจสอบ Ethernet ก่อน
    if (Ethernet.linkStatus() == LinkOFF) {
        Serial.println("Ethernet cable disconnected!");
        digitalWrite(POWER_LED, HIGH);
        delay(5000);
        return;
    }
    
    // ถ้าเชื่อมต่อ Ethernet แล้วแต่ MQTT ยังไม่เชื่อมต่อ
    if (!client.connected()) {
        connectMQTT();
    }
    
    client.loop();
    
    // ... รหัสเดิม ...
}
```

### 9. **เช็คการตั้งค่า IP และเครือข่าย**
โปรดตรวจสอบ:
1. IP Address `172.25.99.58` ไม่ซ้ำกับอุปกรณ์อื่นในเครือข่าย
2. Subnet mask `255.255.255.240` ถูกต้อง
3. Gateway `172.25.99.62` สามารถเข้าถึงได้
4. MQTT Broker `172.25.99.60` ทำงานอยู่และเปิดพอร์ต 1883

ลองรันโค้ดที่แก้ไขแล้วดู error message เพิ่มเติม เพื่อระบุปัญหาให้ชัดเจนยิ่งขึ้น