#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ETH.h>
#include "Arduino.h"
#include <Wire.h>
#include <PCF8574.h>
#include <EEPROM.h>
#include <HardwareSerial.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>

// ------------------ Configuration / Globals ------------------
#define EEPROM_SIZE 512  // เพิ่มขนาด EEPROM สำหรับเก็บข้อมูล schedule

// Pin Definitions
const int buttonPin1 = 36;  // Input 1
const int buttonPin2 = 39;  // Input 2
const int Relay1 = 2;       // Relay 1
const int Relay2 = 15;      // Relay 2
const int Buzzer = 12;      // Buzzer
const int working_led = 32; // Status LED
const int oneWireBus = 33;  // DS18B20 Data pin

// Temperature Sensor
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);
DeviceAddress tempSensorAddress;
bool tempSensorFound = false;
float currentTemperature = 0.0;
const float TEMP_ERROR_VALUE = -127.0;
unsigned long lastTempReadTime = 0;
const long tempReadInterval = 2000;

// Ethernet Configuration
#define ETH_ADDR        0
#define ETH_POWER_PIN  -1
#define ETH_MDC_PIN    23
#define ETH_MDIO_PIN   18
#define ETH_TYPE       ETH_PHY_LAN8720
#define ETH_CLK_MODE   ETH_CLOCK_GPIO17_OUT

// IP Address Configuration
IPAddress local_ip(192, 168, 72, 88);
IPAddress subnet(255, 255, 255, 0);
IPAddress gateway(192, 168, 72, 254);
IPAddress dns(8, 8, 8, 8);

// NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);  // GMT+7

// MQTT Configuration
const char* dataTopic = "MRMONI02122025V5100/DATA";
const char* controlTopic = "MRMONI02122025V5100/CONTROL";
const char* tempTopic = "MRMONI02122025V5100/TEMPERATURE";
const char* scheduleTopic = "MRMONI02122025V5100/SCHEDULE";
const char* mqttServer = "broker.hivemq.com";
const int mqttPort = 1883;
const char* mqttClientID = "MRMONI02122025V5100";

// Web Server Configuration
WebServer server(80);

// Timing Intervals
const long publishInterval = 5000;
const long workingInterval = 1000;
const long buzzerInterval = 500;
const unsigned long debounceDelay = 50;

// Schedule Configuration
const int MAX_SCHEDULES = 6;
const int DAYS_IN_WEEK = 7;

// Day names
const char* dayNames[DAYS_IN_WEEK] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", 
    "Thursday", "Friday", "Saturday"
};

// Schedule structure
struct Schedule {
    bool enabled;
    int relay;          // 1 = Relay1, 2 = Relay2, 3 = Both
    bool action;        // true = ON, false = OFF
    int startHour;
    int startMinute;
    int endHour;
    int endMinute;
    bool days[DAYS_IN_WEEK];  // วันในสัปดาห์ที่เปิดใช้งาน
    bool executedToday;       // เพื่อป้องกันการทำงานซ้ำในวันเดียวกัน
};

Schedule schedules[MAX_SCHEDULES];
int scheduleCount = 0;

// Global Variables
WiFiClient ethClient;
PubSubClient mqttClient(ethClient);

// Input states
int inputState1 = LOW;
int lastInputState1 = LOW;
int stableInputState1 = LOW;
unsigned long lastDebounceTime1 = 0;

int inputState2 = LOW;
int lastInputState2 = LOW;
int stableInputState2 = LOW;
unsigned long lastDebounceTime2 = 0;

// Timing variables
unsigned long lastPublishTime = 0;
unsigned long workingUpdate = 0;
unsigned long buzzerTimer = 0;
bool status_working_led = HIGH;
bool buzzerState = LOW;
bool buzzerActive = false;

// Relay states
bool relay1State = LOW;
bool relay2State = LOW;

// Last update times
String lastInputUpdateTime = "--:--:--";
String lastRelayUpdateTime = "--:--:--";
String lastTempUpdateTime = "--:--:--";

// Current time variables
int currentHour = 0;
int currentMinute = 0;
int currentDay = 0;  // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
String currentDate = "";
unsigned long lastTimeUpdate = 0;
const long timeUpdateInterval = 60000;  // อัพเดทเวลาทุก 1 นาที

// Function Prototypes
void setupEthernet();
void setupTemperatureSensor();
void readTemperature();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void reconnect();
bool debounceRead(int pin, int &lastState, int &stableState, unsigned long &lastDebounceTime);
void readInputs();
void handleBuzzer();
void handleStatusLED();
void sendDataMQTT();
void sendTemperatureMQTT();
void processMQTTCommand(String command);
void controlRelay(int relayNumber, bool state);
void parseRelayCommand(String command);

// Web Server Handlers
void handleRoot();
void handleData();
void handleControl();
void handleSchedules();
void handleSaveSchedule();
void handleDeleteSchedule();
void handleGetSchedule();
void handleNotFound();

// Time and Schedule Functions
void updateCurrentTime();
String getCurrentTime();
String getCurrentDateTime();
void checkSchedules();
void loadSchedules();
void saveSchedules();
void clearAllSchedules();
void printSchedule(int index);

void setup() {
    Serial.begin(115200);
    EEPROM.begin(EEPROM_SIZE);
    
    // Initialize pins
    pinMode(working_led, OUTPUT);
    pinMode(Relay1, OUTPUT);
    pinMode(Relay2, OUTPUT);
    pinMode(Buzzer, OUTPUT);
    pinMode(buttonPin1, INPUT);
    pinMode(buttonPin2, INPUT);
    
    // Initial states
    digitalWrite(working_led, HIGH);
    digitalWrite(Buzzer, LOW);
    digitalWrite(Relay1, LOW);
    digitalWrite(Relay2, LOW);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Setup Ethernet
    setupEthernet();
    
    // Setup NTP Client
    timeClient.begin();
    
    // Load schedules from EEPROM
    loadSchedules();
    
    // Setup Temperature Sensor
    setupTemperatureSensor();
    
    // Setup MQTT
    mqttClient.setServer(mqttServer, mqttPort);
    mqttClient.setCallback(mqttCallback);
    
    // Setup Web Server Routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/data", HTTP_GET, handleData);
    server.on("/control", HTTP_POST, handleControl);
    server.on("/schedules", HTTP_GET, handleSchedules);
    server.on("/save-schedule", HTTP_POST, handleSaveSchedule);
    server.on("/delete-schedule", HTTP_POST, handleDeleteSchedule);
    server.on("/get-schedule", HTTP_GET, handleGetSchedule);
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("HTTP server started");
    Serial.print("Dashboard URL: http://");
    Serial.println(ETH.localIP());
    
    // Update time immediately
    updateCurrentTime();
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Handle Web Server requests
    server.handleClient();
    
    // Maintain MQTT connection
    if (!mqttClient.connected()) {
        reconnect();
    }
    mqttClient.loop();
    
    // Update time from NTP every minute
    if (currentMillis - lastTimeUpdate >= timeUpdateInterval) {
        lastTimeUpdate = currentMillis;
        updateCurrentTime();
    }
    
    // Check schedules every minute
    checkSchedules();
    
    // Handle status LED
    handleStatusLED();
    
    // Handle buzzer if active
    if (buzzerActive) {
        handleBuzzer();
    }
    
    // Read temperature every 2 seconds
    if (currentMillis - lastTempReadTime >= tempReadInterval) {
        lastTempReadTime = currentMillis;
        readTemperature();
        lastTempUpdateTime = getCurrentTime();
        
        if (tempSensorFound) {
            sendTemperatureMQTT();
        }
    }
    
    // Read inputs
    readInputs();
    
    // Publish data every 5 seconds
    if (currentMillis - lastPublishTime >= publishInterval) {
        lastPublishTime = currentMillis;
        sendDataMQTT();
    }
}

// ------------------ Ethernet Setup ------------------
void setupEthernet() {
    Serial.println("Initializing Ethernet...");
    ETH.begin(ETH_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_TYPE, ETH_CLK_MODE);
    
    if (ETH.config(local_ip, gateway, subnet, dns, dns) == false) {
        Serial.println("LAN8720 Configuration failed.");
    } else {
        Serial.println("LAN8720 Configuration success.");
    }
    
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    Serial.print("IP Address: ");
    Serial.println(ETH.localIP());
}

// ------------------ Temperature Sensor Setup ------------------
void setupTemperatureSensor() {
    Serial.println("Initializing DS18B20 temperature sensor...");
    
    sensors.begin();
    
    int deviceCount = sensors.getDeviceCount();
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" devices.");
    
    if (deviceCount > 0) {
        if (sensors.getAddress(tempSensorAddress, 0)) {
            tempSensorFound = true;
            sensors.setResolution(tempSensorAddress, 12);
            sensors.setWaitForConversion(false);
        }
    } else {
        tempSensorFound = false;
    }
    
    if (tempSensorFound) {
        sensors.requestTemperatures();
    }
}

// ------------------ Read Temperature ------------------
void readTemperature() {
    if (!tempSensorFound) {
        currentTemperature = TEMP_ERROR_VALUE;
        return;
    }
    
    if (sensors.isConversionComplete()) {
        float tempC = sensors.getTempC(tempSensorAddress);
        
        if (tempC != DEVICE_DISCONNECTED_C) {
            currentTemperature = tempC;
        } else {
            currentTemperature = TEMP_ERROR_VALUE;
        }
        
        sensors.requestTemperatures();
    }
}

// ------------------ Update Current Time ------------------
void updateCurrentTime() {
    timeClient.update();
    
    // Get current time
    currentHour = timeClient.getHours();
    currentMinute = timeClient.getMinutes();
    currentDay = timeClient.getDay();  // 0 = Sunday, 1 = Monday, etc.
    
    // Get formatted date and time
    currentDate = timeClient.getFormattedDate();
    
    Serial.print("Current time updated: ");
    Serial.print(currentHour);
    Serial.print(":");
    Serial.print(currentMinute);
    Serial.print(" Day: ");
    Serial.println(dayNames[currentDay]);
}

// ------------------ Check Schedules ------------------
void checkSchedules() {
    static int lastCheckedMinute = -1;
    
    // Check only once per minute
    if (currentMinute == lastCheckedMinute) {
        return;
    }
    
    lastCheckedMinute = currentMinute;
    
    for (int i = 0; i < scheduleCount; i++) {
        Schedule& sched = schedules[i];
        
        if (!sched.enabled) continue;
        
        // Check if today is in the schedule days
        if (!sched.days[currentDay]) continue;
        
        // Calculate total minutes for comparison
        int currentTotalMinutes = currentHour * 60 + currentMinute;
        int startTotalMinutes = sched.startHour * 60 + sched.startMinute;
        int endTotalMinutes = sched.endHour * 60 + sched.endMinute;
        
        // For ON action: check if current time is between start and end
        if (sched.action == true) {  // ON action
            if (currentTotalMinutes >= startTotalMinutes && 
                currentTotalMinutes <= endTotalMinutes) {
                if (!sched.executedToday) {
                    // Execute ON action
                    if (sched.relay == 1) {
                        controlRelay(1, true);
                    } else if (sched.relay == 2) {
                        controlRelay(2, true);
                    } else if (sched.relay == 3) {
                        controlRelay(1, true);
                        controlRelay(2, true);
                    }
                    sched.executedToday = true;
                    Serial.print("Schedule ");
                    Serial.print(i);
                    Serial.println(" executed (ON)");
                }
            } else {
                sched.executedToday = false;
            }
        } 
        // For OFF action: check if current time matches start time exactly
        else if (sched.action == false) {  // OFF action
            if (currentHour == sched.startHour && currentMinute == sched.startMinute) {
                if (!sched.executedToday) {
                    // Execute OFF action
                    if (sched.relay == 1) {
                        controlRelay(1, false);
                    } else if (sched.relay == 2) {
                        controlRelay(2, false);
                    } else if (sched.relay == 3) {
                        controlRelay(1, false);
                        controlRelay(2, false);
                    }
                    sched.executedToday = true;
                    Serial.print("Schedule ");
                    Serial.print(i);
                    Serial.println(" executed (OFF)");
                }
            } else if (currentHour == sched.endHour && currentMinute == sched.endMinute) {
                sched.executedToday = false;
            }
        }
    }
}

// ------------------ Load Schedules from EEPROM ------------------
void loadSchedules() {
    int addr = 0;
    EEPROM.get(addr, scheduleCount);
    addr += sizeof(scheduleCount);
    
    if (scheduleCount > MAX_SCHEDULES) {
        scheduleCount = 0;
        return;
    }
    
    for (int i = 0; i < scheduleCount; i++) {
        Schedule sched;
        EEPROM.get(addr, sched);
        addr += sizeof(Schedule);
        schedules[i] = sched;
    }
    
    Serial.print("Loaded ");
    Serial.print(scheduleCount);
    Serial.println(" schedules from EEPROM");
}

// ------------------ Save Schedules to EEPROM ------------------
void saveSchedules() {
    int addr = 0;
    EEPROM.put(addr, scheduleCount);
    addr += sizeof(scheduleCount);
    
    for (int i = 0; i < scheduleCount; i++) {
        EEPROM.put(addr, schedules[i]);
        addr += sizeof(Schedule);
    }
    
    EEPROM.commit();
    Serial.print("Saved ");
    Serial.print(scheduleCount);
    Serial.println(" schedules to EEPROM");
}

// ------------------ Clear All Schedules ------------------
void clearAllSchedules() {
    scheduleCount = 0;
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        memset(&schedules[i], 0, sizeof(Schedule));
    }
    saveSchedules();
}

// ------------------ Print Schedule ------------------
void printSchedule(int index) {
    if (index >= scheduleCount) return;
    
    Schedule& sched = schedules[index];
    Serial.print("Schedule ");
    Serial.print(index);
    Serial.print(": ");
    Serial.print(sched.enabled ? "Enabled" : "Disabled");
    Serial.print(" | Relay: ");
    Serial.print(sched.relay);
    Serial.print(" | Action: ");
    Serial.print(sched.action ? "ON" : "OFF");
    Serial.print(" | Time: ");
    Serial.print(sched.startHour);
    Serial.print(":");
    Serial.print(sched.startMinute);
    Serial.print(" - ");
    Serial.print(sched.endHour);
    Serial.print(":");
    Serial.print(sched.endMinute);
    Serial.print(" | Days: ");
    for (int d = 0; d < DAYS_IN_WEEK; d++) {
        if (sched.days[d]) {
            Serial.print(dayNames[d]);
            Serial.print(" ");
        }
    }
    Serial.println();
}

// ------------------ MQTT Callback ------------------
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    processMQTTCommand(message);
}

// ------------------ MQTT Reconnect ------------------
void reconnect() {
    static unsigned long lastReconnectAttempt = 0;
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastReconnectAttempt >= 5000) {
        lastReconnectAttempt = currentMillis;
        
        Serial.print("MQTT connecting...");
        
        if (mqttClient.connect(mqttClientID)) {
            Serial.println("connected");
            mqttClient.subscribe(controlTopic);
        } else {
            Serial.println("failed");
        }
    }
}

// ------------------ Debounce Function ------------------
bool debounceRead(int pin, int &lastState, int &stableState, unsigned long &lastDebounceTime) {
    int reading = digitalRead(pin);
    unsigned long currentMillis = millis();
    bool stateChanged = false;
    
    if (reading != lastState) {
        lastDebounceTime = currentMillis;
    }
    
    if ((currentMillis - lastDebounceTime) > debounceDelay) {
        if (reading != stableState) {
            stableState = reading;
            stateChanged = true;
            lastInputUpdateTime = getCurrentTime(); // อัพเดทเวลาทุกครั้งที่ Input เปลี่ยน
        }
    }
    
    lastState = reading;
    return stateChanged;
}

// ------------------ Read Inputs ------------------
void readInputs() {
    if (debounceRead(buttonPin1, lastInputState1, stableInputState1, lastDebounceTime1)) {
        if (stableInputState1 == HIGH) {
            buzzerActive = true;
            buzzerTimer = millis();
        }
    }
    
    if (debounceRead(buttonPin2, lastInputState2, stableInputState2, lastDebounceTime2)) {
        if (stableInputState2 == HIGH) {
            buzzerActive = true;
            buzzerTimer = millis();
        }
    }
    
    inputState1 = stableInputState1;
    inputState2 = stableInputState2;
}

// ------------------ Handle Buzzer ------------------
void handleBuzzer() {
    unsigned long currentMillis = millis();
    
    if (currentMillis - buzzerTimer >= buzzerInterval) {
        buzzerTimer = currentMillis;
        buzzerState = !buzzerState;
        digitalWrite(Buzzer, buzzerState ? HIGH : LOW);
        
        static int beepCount = 0;
        beepCount++;
        
        if (beepCount >= 1) {
            buzzerActive = false;
            beepCount = 0;
            digitalWrite(Buzzer, LOW);
        }
    }
}

// ------------------ Handle Status LED ------------------
void handleStatusLED() {
    unsigned long currentMillis = millis();
    
    if (currentMillis - workingUpdate >= workingInterval) {
        workingUpdate = currentMillis;
        status_working_led = !status_working_led;
        digitalWrite(working_led, status_working_led);
    }
}

// ------------------ Send Data via MQTT ------------------
void sendDataMQTT() {
    char msgBuffer[300];
    snprintf(
        msgBuffer,
        sizeof(msgBuffer),
        "{\"input1\":%d,\"input2\":%d,\"relay1\":%d,\"relay2\":%d,\"temperature\":%.2f,\"temp_sensor\":%s,\"ip\":\"%s\",\"time\":\"%02d:%02d\",\"schedules\":%d}",
        inputState1,
        inputState2,
        relay1State,
        relay2State,
        currentTemperature,
        tempSensorFound ? "true" : "false",
        ETH.localIP().toString().c_str(),
        currentHour,
        currentMinute,
        scheduleCount
    );
    
    mqttClient.publish(dataTopic, msgBuffer);
}

// ------------------ Send Temperature via MQTT ------------------
void sendTemperatureMQTT() {
    char tempMsg[150];
    snprintf(
        tempMsg,
        sizeof(tempMsg),
        "{\"temperature\":%.2f,\"unit\":\"C\",\"sensor_status\":%s}",
        currentTemperature,
        tempSensorFound ? "\"OK\"" : "\"ERROR\""
    );
    
    mqttClient.publish(tempTopic, tempMsg);
}

// ------------------ Control Relay Function ------------------
void controlRelay(int relayNumber, bool state) {
    if (relayNumber == 1) {
        digitalWrite(Relay1, state ? HIGH : LOW);
        relay1State = state;
        lastRelayUpdateTime = getCurrentTime(); // อัพเดทเวลาทุกครั้งที่ควบคุมรีเลย์
    } 
    else if (relayNumber == 2) {
        digitalWrite(Relay2, state ? HIGH : LOW);
        relay2State = state;
        lastRelayUpdateTime = getCurrentTime(); // อัพเดทเวลาทุกครั้งที่ควบคุมรีเลย์
    }
    
    buzzerActive = true;
    buzzerTimer = millis();
}

// ------------------ Parse Relay Command ------------------
void parseRelayCommand(String command) {
    command.trim();
    command.toUpperCase();
    command.replace(" ", "");
    
    if (command == "RELAY1=ON") {
        controlRelay(1, true);
    }
    else if (command == "RELAY1=OFF") {
        controlRelay(1, false);
    }
    else if (command == "RELAY2=ON") {
        controlRelay(2, true);
    }
    else if (command == "RELAY2=OFF") {
        controlRelay(2, false);
    }
    else if (command == "ALL=ON" || command == "ALL_ON") {
        controlRelay(1, true);
        controlRelay(2, true);
    }
    else if (command == "ALL=OFF" || command == "ALL_OFF") {
        controlRelay(1, false);
        controlRelay(2, false);
    }
}

// ------------------ Process MQTT Commands ------------------
void processMQTTCommand(String command) {
    String cmdUpper = command;
    cmdUpper.trim();
    cmdUpper.toUpperCase();
    
    if (cmdUpper.startsWith("RELAY1=") || cmdUpper.startsWith("RELAY2=") || 
        cmdUpper == "ALL=ON" || cmdUpper == "ALL=OFF" ||
        cmdUpper == "ALL_ON" || cmdUpper == "ALL_OFF") {
        parseRelayCommand(command);
        return;
    }
    
    if (command == "10") controlRelay(1, false);
    else if (command == "11") controlRelay(1, true);
    else if (command == "20") controlRelay(2, false);
    else if (command == "21") controlRelay(2, true);
}

// ------------------ Get Current Time ------------------
String getCurrentTime() {
    char buffer[9];
    sprintf(buffer, "%02d:%02d:%02d", currentHour, currentMinute, timeClient.getSeconds());
    return String(buffer);
}

// ------------------ Get Current Date Time ------------------
String getCurrentDateTime() {
    return currentDate + " " + getCurrentTime();
}

// ------------------ Web Server Handlers ------------------
void handleRoot() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 IoT Dashboard - Schedule Control</title>
    <style>
        * { 
            margin: 0; 
            padding: 0; 
            box-sizing: border-box; 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        body { 
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 20px;
        }
        .container {
            max-width: 1400px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        .header {
            background: linear-gradient(135deg, #4f46e5 0%, #7c3aed 100%);
            color: white;
            padding: 25px 40px;
            text-align: center;
        }
        .header h1 {
            font-size: 2.2rem;
            margin-bottom: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 15px;
        }
        .header p {
            opacity: 0.9;
            font-size: 1.1rem;
        }
        .tabs {
            display: flex;
            background: #f1f5f9;
            border-bottom: 2px solid #e2e8f0;
        }
        .tab {
            padding: 15px 30px;
            cursor: pointer;
            font-weight: 600;
            color: #64748b;
            transition: all 0.3s;
            border-bottom: 3px solid transparent;
        }
        .tab:hover {
            background: #e2e8f0;
            color: #475569;
        }
        .tab.active {
            background: white;
            color: #4f46e5;
            border-bottom-color: #4f46e5;
        }
        .tab-content {
            display: none;
            padding: 30px;
        }
        .tab-content.active {
            display: block;
        }
        .dashboard {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 25px;
            margin-bottom: 30px;
        }
        .card {
            background: white;
            border-radius: 15px;
            padding: 30px;
            box-shadow: 0 10px 25px rgba(0,0,0,0.08);
            border: 1px solid #e5e7eb;
            transition: transform 0.3s ease;
        }
        .card:hover {
            transform: translateY(-5px);
        }
        .card-title {
            color: #4f46e5;
            margin-bottom: 25px;
            font-size: 1.3rem;
            display: flex;
            align-items: center;
            gap: 12px;
        }
        .card-title i {
            font-size: 1.5rem;
        }
        .temperature-display {
            text-align: center;
            padding: 20px 0;
        }
        .temp-value {
            font-size: 4rem;
            font-weight: 700;
            color: #dc2626;
            margin: 10px 0;
            font-family: 'Courier New', monospace;
        }
        .temp-unit {
            font-size: 2rem;
            color: #6b7280;
            margin-left: 5px;
        }
        .temp-status {
            display: inline-block;
            padding: 8px 20px;
            border-radius: 20px;
            background: #f3f4f6;
            font-size: 0.9rem;
            margin-top: 15px;
        }
        .status-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-bottom: 25px;
        }
        .status-item {
            text-align: center;
            padding: 20px;
            border-radius: 12px;
            background: #f9fafb;
            border: 1px solid #e5e7eb;
        }
        .status-label {
            font-size: 0.9rem;
            color: #6b7280;
            margin-bottom: 8px;
            font-weight: 500;
        }
        .status-value {
            font-size: 2.2rem;
            font-weight: 700;
            margin: 10px 0;
            font-family: 'Courier New', monospace;
        }
        .status-on {
            color: #10b981;
        }
        .status-off {
            color: #ef4444;
        }
        .control-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
            margin-top: 20px;
        }
        .btn {
            padding: 16px;
            border: none;
            border-radius: 10px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
        }
        .btn i {
            font-size: 1.2rem;
        }
        .btn-on {
            background: linear-gradient(135deg, #10b981, #34d399);
            color: white;
        }
        .btn-on:hover {
            background: linear-gradient(135deg, #0da271, #2bb987);
            transform: scale(1.05);
        }
        .btn-off {
            background: linear-gradient(135deg, #ef4444, #f87171);
            color: white;
        }
        .btn-off:hover {
            background: linear-gradient(135deg, #dc2626, #e85c5c);
            transform: scale(1.05);
        }
        .btn-all {
            grid-column: span 2;
            background: linear-gradient(135deg, #8b5cf6, #a78bfa);
            color: white;
            padding: 18px;
            margin-top: 10px;
        }
        .btn-all:hover {
            background: linear-gradient(135deg, #7c3aed, #946de3);
            transform: scale(1.05);
        }
        .last-update {
            text-align: center;
            margin-top: 20px;
            padding-top: 15px;
            border-top: 1px solid #e5e7eb;
            color: #6b7280;
            font-size: 0.9rem;
        }
        .last-update span {
            font-weight: 600;
            color: #4f46e5;
        }
        .connection-info {
            margin-top: 20px;
            padding: 15px;
            background: #f8fafc;
            border-radius: 10px;
            text-align: center;
        }
        .info-item {
            margin: 5px 0;
        }
        .info-label {
            font-size: 0.85rem;
            color: #6b7280;
        }
        .info-value {
            font-weight: 600;
            color: #1f2937;
        }
        .refresh-btn {
            background: #3b82f6;
            color: white;
            border: none;
            padding: 12px 24px;
            border-radius: 8px;
            cursor: pointer;
            font-weight: 600;
            margin-top: 15px;
            display: inline-flex;
            align-items: center;
            gap: 8px;
            transition: all 0.3s;
        }
        .refresh-btn:hover {
            background: #2563eb;
            transform: scale(1.05);
        }
        
        /* Schedule Form Styles */
        .schedule-form {
            background: white;
            border-radius: 15px;
            padding: 30px;
            margin-bottom: 30px;
            box-shadow: 0 5px 15px rgba(0,0,0,0.05);
            border: 1px solid #e5e7eb;
        }
        .form-group {
            margin-bottom: 20px;
        }
        .form-row {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 15px;
        }
        .form-label {
            display: block;
            margin-bottom: 8px;
            font-weight: 600;
            color: #374151;
        }
        .form-control {
            width: 100%;
            padding: 12px 15px;
            border: 1px solid #d1d5db;
            border-radius: 8px;
            font-size: 1rem;
            transition: border 0.3s;
        }
        .form-control:focus {
            outline: none;
            border-color: #4f46e5;
            box-shadow: 0 0 0 3px rgba(79, 70, 229, 0.1);
        }
        .form-select {
            width: 100%;
            padding: 12px 15px;
            border: 1px solid #d1d5db;
            border-radius: 8px;
            font-size: 1rem;
            background: white;
            cursor: pointer;
        }
        .checkbox-group {
            display: flex;
            flex-wrap: wrap;
            gap: 15px;
            margin-top: 10px;
        }
        .checkbox-label {
            display: flex;
            align-items: center;
            gap: 8px;
            cursor: pointer;
        }
        .checkbox-label input[type="checkbox"] {
            width: 18px;
            height: 18px;
            cursor: pointer;
        }
        .time-inputs {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .time-inputs span {
            color: #6b7280;
        }
        .form-actions {
            display: flex;
            gap: 15px;
            margin-top: 30px;
        }
        .btn-primary {
            background: linear-gradient(135deg, #4f46e5, #7c3aed);
            color: white;
            border: none;
            padding: 15px 30px;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            flex: 1;
        }
        .btn-primary:hover {
            background: linear-gradient(135deg, #4338ca, #6d28d9);
            transform: translateY(-2px);
        }
        .btn-secondary {
            background: #6b7280;
            color: white;
            border: none;
            padding: 15px 30px;
            border-radius: 8px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            flex: 1;
        }
        .btn-secondary:hover {
            background: #4b5563;
            transform: translateY(-2px);
        }
        
        /* Schedule List Styles */
        .schedule-list {
            margin-top: 30px;
        }
        .schedule-item {
            background: white;
            border: 1px solid #e5e7eb;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 15px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            transition: all 0.3s;
        }
        .schedule-item:hover {
            box-shadow: 0 5px 15px rgba(0,0,0,0.08);
        }
        .schedule-info {
            flex: 1;
        }
        .schedule-title {
            font-size: 1.1rem;
            font-weight: 600;
            color: #1f2937;
            margin-bottom: 5px;
        }
        .schedule-details {
            color: #6b7280;
            font-size: 0.9rem;
            margin-bottom: 5px;
        }
        .schedule-days {
            display: flex;
            gap: 5px;
            flex-wrap: wrap;
        }
        .day-tag {
            background: #e0e7ff;
            color: #4f46e5;
            padding: 3px 8px;
            border-radius: 12px;
            font-size: 0.8rem;
            font-weight: 500;
        }
        .schedule-status {
            display: flex;
            align-items: center;
            gap: 10px;
            margin-top: 10px;
        }
        .status-badge {
            padding: 5px 12px;
            border-radius: 15px;
            font-size: 0.8rem;
            font-weight: 600;
        }
        .status-active {
            background: #d1fae5;
            color: #065f46;
        }
        .status-inactive {
            background: #fee2e2;
            color: #991b1b;
        }
        .schedule-actions {
            display: flex;
            gap: 10px;
        }
        .btn-edit {
            background: #3b82f6;
            color: white;
            border: none;
            padding: 8px 15px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 0.9rem;
        }
        .btn-delete {
            background: #ef4444;
            color: white;
            border: none;
            padding: 8px 15px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 0.9rem;
        }
        .btn-edit:hover {
            background: #2563eb;
        }
        .btn-delete:hover {
            background: #dc2626;
        }
        
        .empty-state {
            text-align: center;
            padding: 50px 20px;
            color: #6b7280;
        }
        .empty-state i {
            font-size: 3rem;
            margin-bottom: 15px;
            color: #d1d5db;
        }
        
        .current-time {
            text-align: center;
            padding: 15px;
            background: linear-gradient(135deg, #10b981, #34d399);
            color: white;
            border-radius: 10px;
            margin-bottom: 20px;
            font-size: 1.2rem;
            font-weight: 600;
        }
        
        @media (max-width: 768px) {
            .dashboard {
                grid-template-columns: 1fr;
                padding: 20px;
            }
            .header h1 {
                font-size: 1.8rem;
            }
            .temp-value {
                font-size: 3rem;
            }
            .tabs {
                flex-wrap: wrap;
            }
            .tab {
                flex: 1;
                min-width: 120px;
                text-align: center;
            }
            .form-row {
                grid-template-columns: 1fr;
            }
            .schedule-item {
                flex-direction: column;
                align-items: flex-start;
                gap: 15px;
            }
            .schedule-actions {
                align-self: flex-end;
            }
        }
    </style>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <script>
        let isUpdating = false;
        let currentTab = 'dashboard';
        let editingIndex = -1;
        
        // Tab switching
        function switchTab(tabName) {
            currentTab = tabName;
            
            // Update active tab
            document.querySelectorAll('.tab').forEach(tab => {
                tab.classList.remove('active');
                if (tab.dataset.tab === tabName) {
                    tab.classList.add('active');
                }
            });
            
            // Update active content
            document.querySelectorAll('.tab-content').forEach(content => {
                content.classList.remove('active');
                if (content.id === tabName + 'Tab') {
                    content.classList.add('active');
                }
            });
            
            // If switching to schedules tab, load schedules
            if (tabName === 'schedules') {
                loadSchedules();
            }
        }
        
        // Dashboard functions
        async function fetchData() {
            if (isUpdating) return;
            isUpdating = true;
            
            try {
                const response = await fetch('/data');
                if (!response.ok) throw new Error('Network error');
                const data = await response.json();
                updateDashboard(data);
            } catch (error) {
                console.error('Error:', error);
                document.getElementById('tempStatus').innerHTML = 
                    '<i class="fas fa-exclamation-circle"></i> Connection Error';
                document.getElementById('tempStatus').style.color = '#ef4444';
            } finally {
                isUpdating = false;
            }
        }
        
        function updateDashboard(data) {
            // Update temperature
            const tempElement = document.getElementById('temperatureValue');
            const tempStatus = document.getElementById('tempStatus');
            
            if (data.temperature !== undefined && data.temperature !== -127) {
                tempElement.textContent = data.temperature.toFixed(1);
                tempStatus.innerHTML = '<i class="fas fa-check-circle"></i> Sensor OK';
                tempStatus.style.color = '#10b981';
                document.getElementById('tempUpdateTime').textContent = data.temp_update_time || 'Now';
            } else {
                tempElement.textContent = '--.-';
                tempStatus.innerHTML = '<i class="fas fa-exclamation-triangle"></i> Sensor Error';
                tempStatus.style.color = '#ef4444';
            }
            
            // Update inputs
            updateStatus('input1Status', data.input1, 'Input 1');
            updateStatus('input2Status', data.input2, 'Input 2');
            
            // Update relays
            updateStatus('relay1Status', data.relay1, 'Relay 1');
            updateStatus('relay2Status', data.relay2, 'Relay 2');
            
            // Update times
            document.getElementById('inputUpdateTime').textContent = data.input_update_time || 'Now';
            document.getElementById('relayUpdateTime').textContent = data.relay_update_time || 'Now';
            
            // Update connection info
            document.getElementById('ipAddress').textContent = data.ip || '192.168.72.88';
            document.getElementById('currentTime').textContent = data.time || '--:--';
            document.getElementById('scheduleCount').textContent = data.schedules || 0;
        }
        
        function updateStatus(elementId, status, label) {
            const element = document.getElementById(elementId);
            if (status === 1 || status === true) {
                element.textContent = 'ON';
                element.className = 'status-value status-on';
            } else {
                element.textContent = 'OFF';
                element.className = 'status-value status-off';
            }
        }
        
        async function sendCommand(command) {
            try {
                const response = await fetch('/control', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/x-www-form-urlencoded',
                    },
                    body: 'cmd=' + encodeURIComponent(command)
                });
                
                const result = await response.json();
                console.log('Command result:', result);
                
                // Refresh data immediately after command
                setTimeout(fetchData, 500);
                
            } catch (error) {
                console.error('Command error:', error);
                alert('Command failed: ' + error.message);
            }
        }
        
        function controlRelay(relayNumber, state) {
            const command = state ? `Relay${relayNumber}=ON` : `Relay${relayNumber}=OFF`;
            sendCommand(command);
        }
        
        function controlAllRelays(state) {
            const command = state ? 'ALL=ON' : 'ALL=OFF';
            sendCommand(command);
        }
        
        // Schedule functions
        async function loadSchedules() {
            try {
                const response = await fetch('/schedules');
                if (!response.ok) throw new Error('Network error');
                const data = await response.json();
                displaySchedules(data);
            } catch (error) {
                console.error('Error loading schedules:', error);
                document.getElementById('scheduleList').innerHTML = 
                    '<div class="empty-state"><i class="fas fa-exclamation-circle"></i><p>Error loading schedules</p></div>';
            }
        }
        
        function displaySchedules(data) {
            const scheduleList = document.getElementById('scheduleList');
            
            if (!data.schedules || data.schedules.length === 0) {
                scheduleList.innerHTML = `
                    <div class="empty-state">
                        <i class="far fa-calendar-plus"></i>
                        <h3>No schedules yet</h3>
                        <p>Click "Add New Schedule" to create your first schedule</p>
                    </div>
                `;
                return;
            }
            
            let html = '';
            data.schedules.forEach((schedule, index) => {
                const days = schedule.days.map((day, i) => day ? data.dayNames[i] : '').filter(day => day);
                const dayTags = days.map(day => `<span class="day-tag">${day}</span>`).join('');
                
                html += `
                    <div class="schedule-item">
                        <div class="schedule-info">
                            <div class="schedule-title">
                                ${schedule.action === 'ON' ? 'Turn ON' : 'Turn OFF'} 
                                ${schedule.relay === 1 ? 'Relay 1' : schedule.relay === 2 ? 'Relay 2' : 'Both Relays'}
                            </div>
                            <div class="schedule-details">
                                Time: ${schedule.startHour.toString().padStart(2, '0')}:${schedule.startMinute.toString().padStart(2, '0')}
                                ${schedule.action === 'ON' ? ` to ${schedule.endHour.toString().padStart(2, '0')}:${schedule.endMinute.toString().padStart(2, '0')}` : ''}
                            </div>
                            <div class="schedule-days">${dayTags}</div>
                            <div class="schedule-status">
                                <span class="status-badge ${schedule.enabled ? 'status-active' : 'status-inactive'}">
                                    ${schedule.enabled ? 'Active' : 'Inactive'}
                                </span>
                            </div>
                        </div>
                        <div class="schedule-actions">
                            <button class="btn-edit" onclick="editSchedule(${index})">
                                <i class="fas fa-edit"></i> Edit
                            </button>
                            <button class="btn-delete" onclick="deleteSchedule(${index})">
                                <i class="fas fa-trash"></i> Delete
                            </button>
                        </div>
                    </div>
                `;
            });
            
            scheduleList.innerHTML = html;
        }
        
        async function saveSchedule() {
            const form = document.getElementById('scheduleForm');
            const formData = new FormData(form);
            
            // Validate form
            if (!formData.get('relay')) {
                alert('Please select a relay');
                return;
            }
            
            if (!formData.get('action')) {
                alert('Please select an action');
                return;
            }
            
            // Collect selected days
            const days = [];
            for (let i = 0; i < 7; i++) {
                days.push(formData.get(`day${i}`) === 'on');
            }
            
            // Prepare schedule data
            const scheduleData = {
                enabled: formData.get('enabled') === 'on',
                relay: parseInt(formData.get('relay')),
                action: formData.get('action') === 'ON',
                startHour: parseInt(formData.get('startHour')),
                startMinute: parseInt(formData.get('startMinute')),
                endHour: parseInt(formData.get('endHour')),
                endMinute: parseInt(formData.get('endMinute')),
                days: days,
                index: editingIndex
            };
            
            try {
                const response = await fetch('/save-schedule', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify(scheduleData)
                });
                
                const result = await response.json();
                
                if (result.status === 'success') {
                    alert('Schedule saved successfully!');
                    resetForm();
                    loadSchedules();
                    
                    // If editing, switch back to schedule list
                    if (editingIndex !== -1) {
                        editingIndex = -1;
                        document.getElementById('formTitle').textContent = 'Add New Schedule';
                        document.getElementById('submitBtn').innerHTML = '<i class="fas fa-plus"></i> Add Schedule';
                    }
                } else {
                    alert('Error saving schedule: ' + result.message);
                }
            } catch (error) {
                console.error('Error saving schedule:', error);
                alert('Error saving schedule: ' + error.message);
            }
        }
        
        async function editSchedule(index) {
            try {
                const response = await fetch('/get-schedule?index=' + index);
                if (!response.ok) throw new Error('Network error');
                const schedule = await response.json();
                
                // Populate form with schedule data
                document.getElementById('enabled').checked = schedule.enabled;
                document.getElementById('relay').value = schedule.relay;
                document.getElementById('action').value = schedule.action ? 'ON' : 'OFF';
                document.getElementById('startHour').value = schedule.startHour;
                document.getElementById('startMinute').value = schedule.startMinute;
                document.getElementById('endHour').value = schedule.endHour;
                document.getElementById('endMinute').value = schedule.endMinute;
                
                // Set days
                for (let i = 0; i < 7; i++) {
                    document.getElementById(`day${i}`).checked = schedule.days[i];
                }
                
                // Update form title and button
                editingIndex = index;
                document.getElementById('formTitle').textContent = 'Edit Schedule';
                document.getElementById('submitBtn').innerHTML = '<i class="fas fa-save"></i> Update Schedule';
                
                // Scroll to form
                document.getElementById('scheduleForm').scrollIntoView({ behavior: 'smooth' });
                
            } catch (error) {
                console.error('Error loading schedule for edit:', error);
                alert('Error loading schedule: ' + error.message);
            }
        }
        
        async function deleteSchedule(index) {
            if (!confirm('Are you sure you want to delete this schedule?')) {
                return;
            }
            
            try {
                const response = await fetch('/delete-schedule', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                    },
                    body: JSON.stringify({ index: index })
                });
                
                const result = await response.json();
                
                if (result.status === 'success') {
                    alert('Schedule deleted successfully!');
                    loadSchedules();
                } else {
                    alert('Error deleting schedule: ' + result.message);
                }
            } catch (error) {
                console.error('Error deleting schedule:', error);
                alert('Error deleting schedule: ' + error.message);
            }
        }
        
        function resetForm() {
            document.getElementById('scheduleForm').reset();
            editingIndex = -1;
            document.getElementById('formTitle').textContent = 'Add New Schedule';
            document.getElementById('submitBtn').innerHTML = '<i class="fas fa-plus"></i> Add Schedule';
        }
        
        // Auto-refresh dashboard every 3 seconds
        setInterval(fetchData, 3000);
        
        // Initial setup
        document.addEventListener('DOMContentLoaded', function() {
            fetchData();
            updateCurrentTimeDisplay();
            setInterval(updateCurrentTimeDisplay, 60000); // Update time every minute
        });
        
        function updateCurrentTimeDisplay() {
            const now = new Date();
            const timeString = now.getHours().toString().padStart(2, '0') + ':' + 
                             now.getMinutes().toString().padStart(2, '0');
            document.getElementById('currentTimeDisplay').textContent = timeString;
        }
    </script>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>
                <i class="fas fa-microchip"></i>
                ESP32 IoT Dashboard - Schedule Control
            </h1>
            <p>Device: MROMONI02122025V5001 | IP: <span id="ipAddress">192.168.72.88</span></p>
            <div class="current-time">
                <i class="fas fa-clock"></i>
                Current Time: <span id="currentTimeDisplay">--:--</span>
            </div>
        </div>
        
        <div class="tabs">
            <div class="tab active" data-tab="dashboard" onclick="switchTab('dashboard')">
                <i class="fas fa-tachometer-alt"></i> Dashboard
            </div>
            <div class="tab" data-tab="schedules" onclick="switchTab('schedules')">
                <i class="fas fa-calendar-alt"></i> Schedules
                <span id="scheduleCount" style="margin-left: 8px; background: white; color: #4f46e5; padding: 2px 8px; border-radius: 10px;">0</span>
            </div>
        </div>
        
        <!-- Dashboard Tab -->
        <div id="dashboardTab" class="tab-content active">
            <div class="dashboard">
                <!-- Temperature Card -->
                <div class="card">
                    <div class="card-title">
                        <i class="fas fa-thermometer-half"></i>
                        <h2>Temperature</h2>
                    </div>
                    <div class="temperature-display">
                        <div id="temperatureValue" class="temp-value">--.-</div>
                        <div class="temp-unit">°C</div>
                        <div id="tempStatus" class="temp-status">
                            <i class="fas fa-spinner fa-spin"></i> Loading...
                        </div>
                    </div>
                    <div class="last-update">
                        Updated: <span id="tempUpdateTime">--:--:--</span>
                    </div>
                </div>
                
                <!-- Input Status Card -->
                <div class="card">
                    <div class="card-title">
                        <i class="fas fa-signal"></i>
                        <h2>Input Status</h2>
                    </div>
                    <div class="status-grid">
                        <div class="status-item">
                            <div class="status-label">INPUT 1</div>
                            <div id="input1Status" class="status-value status-off">OFF</div>
                            <div class="status-label">GPIO 36</div>
                        </div>
                        <div class="status-item">
                            <div class="status-label">INPUT 2</div>
                            <div id="input2Status" class="status-value status-off">OFF</div>
                            <div class="status-label">GPIO 39</div>
                        </div>
                    </div>
                    <div class="last-update">
                        Updated: <span id="inputUpdateTime">--:--:--</span>
                    </div>
                </div>
                
                <!-- Relay Control Card -->
                <div class="card">
                    <div class="card-title">
                        <i class="fas fa-bolt"></i>
                        <h2>Relay Control</h2>
                    </div>
                    <div class="status-grid">
                        <div class="status-item">
                            <div class="status-label">RELAY 1</div>
                            <div id="relay1Status" class="status-value status-off">OFF</div>
                            <div class="status-label">GPIO 2</div>
                        </div>
                        <div class="status-item">
                            <div class="status-label">RELAY 2</div>
                            <div id="relay2Status" class="status-value status-off">OFF</div>
                            <div class="status-label">GPIO 15</div>
                        </div>
                    </div>
                    
                    <div class="control-buttons">
                        <button class="btn btn-on" onclick="controlRelay(1, true)">
                            <i class="fas fa-power-off"></i>
                            Relay 1 ON
                        </button>
                        <button class="btn btn-off" onclick="controlRelay(1, false)">
                            <i class="fas fa-power-off"></i>
                            Relay 1 OFF
                        </button>
                        <button class="btn btn-on" onclick="controlRelay(2, true)">
                            <i class="fas fa-power-off"></i>
                            Relay 2 ON
                        </button>
                        <button class="btn btn-off" onclick="controlRelay(2, false)">
                            <i class="fas fa-power-off"></i>
                            Relay 2 OFF
                        </button>
                        <button class="btn btn-all" onclick="controlAllRelays(true)">
                            <i class="fas fa-bolt"></i>
                            ALL ON
                        </button>
                        <button class="btn btn-all" onclick="controlAllRelays(false)">
                            <i class="fas fa-ban"></i>
                            ALL OFF
                        </button>
                    </div>
                    
                    <div class="last-update">
                        Updated: <span id="relayUpdateTime">--:--:--</span>
                    </div>
                </div>
                
                <!-- Connection Info Card -->
                <div class="card">
                    <div class="card-title">
                        <i class="fas fa-wifi"></i>
                        <h2>Connection Info</h2>
                    </div>
                    <div class="connection-info">
                        <div class="info-item">
                            <div class="info-label">Current Time</div>
                            <div class="info-value" id="currentTime">--:--</div>
                        </div>
                        <div class="info-item">
                            <div class="info-label">Active Schedules</div>
                            <div class="info-value" id="scheduleCount2">0</div>
                        </div>
                        <div class="info-item">
                            <div class="info-label">Web Server</div>
                            <div class="info-value">Port 80</div>
                        </div>
                        <button class="refresh-btn" onclick="fetchData()">
                            <i class="fas fa-sync-alt"></i>
                            Refresh Data
                        </button>
                    </div>
                    <div class="last-update">
                        Auto-refresh every 3 seconds
                    </div>
                </div>
            </div>
        </div>
        
        <!-- Schedules Tab -->
        <div id="schedulesTab" class="tab-content">
            <div class="schedule-form">
                <h2 id="formTitle" class="card-title">
                    <i class="fas fa-calendar-plus"></i>
                    Add New Schedule
                </h2>
                
                <form id="scheduleForm">
                    <div class="form-row">
                        <div class="form-group">
                            <label class="form-label">Enable Schedule</label>
                            <label class="checkbox-label">
                                <input type="checkbox" id="enabled" name="enabled" checked>
                                <span>Active</span>
                            </label>
                        </div>
                        
                        <div class="form-group">
                            <label class="form-label" for="relay">Relay</label>
                            <select class="form-select" id="relay" name="relay" required>
                                <option value="">Select Relay</option>
                                <option value="1">Relay 1</option>
                                <option value="2">Relay 2</option>
                                <option value="3">Both Relays</option>
                            </select>
                        </div>
                        
                        <div class="form-group">
                            <label class="form-label" for="action">Action</label>
                            <select class="form-select" id="action" name="action" required>
                                <option value="">Select Action</option>
                                <option value="ON">Turn ON</option>
                                <option value="OFF">Turn OFF</option>
                            </select>
                        </div>
                    </div>
                    
                    <div class="form-row">
                        <div class="form-group">
                            <label class="form-label">Start Time</label>
                            <div class="time-inputs">
                                <input type="number" class="form-control" id="startHour" name="startHour" 
                                       min="0" max="23" placeholder="HH" required>
                                <span>:</span>
                                <input type="number" class="form-control" id="startMinute" name="startMinute" 
                                       min="0" max="59" placeholder="MM" required>
                            </div>
                        </div>
                        
                        <div class="form-group">
                            <label class="form-label">End Time (for ON action)</label>
                            <div class="time-inputs">
                                <input type="number" class="form-control" id="endHour" name="endHour" 
                                       min="0" max="23" placeholder="HH">
                                <span>:</span>
                                <input type="number" class="form-control" id="endMinute" name="endMinute" 
                                       min="0" max="59" placeholder="MM">
                            </div>
                            <small style="color: #6b7280; font-size: 0.8rem;">Note: End time is only used for ON action</small>
                        </div>
                    </div>
                    
                    <div class="form-group">
                        <label class="form-label">Days of Week</label>
                        <div class="checkbox-group">
                            <label class="checkbox-label">
                                <input type="checkbox" id="day0" name="day0">
                                <span>Sun</span>
                            </label>
                            <label class="checkbox-label">
                                <input type="checkbox" id="day1" name="day1">
                                <span>Mon</span>
                            </label>
                            <label class="checkbox-label">
                                <input type="checkbox" id="day2" name="day2">
                                <span>Tue</span>
                            </label>
                            <label class="checkbox-label">
                                <input type="checkbox" id="day3" name="day3">
                                <span>Wed</span>
                            </label>
                            <label class="checkbox-label">
                                <input type="checkbox" id="day4" name="day4">
                                <span>Thu</span>
                            </label>
                            <label class="checkbox-label">
                                <input type="checkbox" id="day5" name="day5">
                                <span>Fri</span>
                            </label>
                            <label class="checkbox-label">
                                <input type="checkbox" id="day6" name="day6">
                                <span>Sat</span>
                            </label>
                        </div>
                    </div>
                    
                    <div class="form-actions">
                        <button type="button" class="btn-primary" onclick="saveSchedule()" id="submitBtn">
                            <i class="fas fa-plus"></i> Add Schedule
                        </button>
                        <button type="button" class="btn-secondary" onclick="resetForm()">
                            <i class="fas fa-redo"></i> Reset Form
                        </button>
                    </div>
                </form>
            </div>
            
            <div class="schedule-list">
                <h2 class="card-title">
                    <i class="fas fa-list"></i>
                    Schedule List (Max: 6 schedules)
                </h2>
                <div id="scheduleList">
                    <!-- Schedules will be loaded here -->
                    <div class="empty-state">
                        <i class="fas fa-spinner fa-spin"></i>
                        <p>Loading schedules...</p>
                    </div>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";
    
    server.send(200, "text/html", html);
}

void handleData() {
    // Create JSON response with update times
    String json = "{";
    json += "\"input1\":" + String(inputState1) + ",";
    json += "\"input2\":" + String(inputState2) + ",";
    json += "\"relay1\":" + String(relay1State) + ",";
    json += "\"relay2\":" + String(relay2State) + ",";
    json += "\"temperature\":" + String(currentTemperature, 1) + ",";
    json += "\"temp_sensor\":" + String(tempSensorFound ? "true" : "false") + ",";
    json += "\"input_update_time\":\"" + lastInputUpdateTime + "\",";
    json += "\"relay_update_time\":\"" + lastRelayUpdateTime + "\",";
    json += "\"temp_update_time\":\"" + lastTempUpdateTime + "\",";
    json += "\"time\":\"" + String(currentHour) + ":" + String(currentMinute) + "\",";
    json += "\"schedules\":" + String(scheduleCount) + ",";
    json += "\"ip\":\"" + ETH.localIP().toString() + "\"";
    json += "}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", json);
}

void handleControl() {
    if (server.hasArg("cmd")) {
        String command = server.arg("cmd");
        
        processMQTTCommand(command);
        
        // Send response
        String response = "{";
        response += "\"status\":\"success\",";
        response += "\"command\":\"" + command + "\",";
        response += "\"relay1\":" + String(relay1State) + ",";
        response += "\"relay2\":" + String(relay2State);
        response += "}";
        
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(200, "application/json", response);
    } else {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No command\"}");
    }
}

void handleSchedules() {
    String json = "{";
    json += "\"status\":\"success\",";
    json += "\"count\":" + String(scheduleCount) + ",";
    json += "\"maxSchedules\":" + String(MAX_SCHEDULES) + ",";
    json += "\"dayNames\":[\"Sunday\",\"Monday\",\"Tuesday\",\"Wednesday\",\"Thursday\",\"Friday\",\"Saturday\"],";
    json += "\"schedules\":[";
    
    for (int i = 0; i < scheduleCount; i++) {
        Schedule& sched = schedules[i];
        
        json += "{";
        json += "\"index\":" + String(i) + ",";
        json += "\"enabled\":" + String(sched.enabled ? "true" : "false") + ",";
        json += "\"relay\":" + String(sched.relay) + ",";
        json += "\"action\":\"" + String(sched.action ? "ON" : "OFF") + "\",";
        json += "\"startHour\":" + String(sched.startHour) + ",";
        json += "\"startMinute\":" + String(sched.startMinute) + ",";
        json += "\"endHour\":" + String(sched.endHour) + ",";
        json += "\"endMinute\":" + String(sched.endMinute) + ",";
        json += "\"days\":[";
        
        for (int d = 0; d < DAYS_IN_WEEK; d++) {
            json += String(sched.days[d] ? "true" : "false");
            if (d < DAYS_IN_WEEK - 1) json += ",";
        }
        
        json += "]";
        json += "}";
        
        if (i < scheduleCount - 1) json += ",";
    }
    
    json += "]}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", json);
}

void handleSaveSchedule() {
    String body = server.arg("plain");
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    int index = doc["index"] | -1;
    bool isEdit = (index >= 0 && index < scheduleCount);
    
    // Check if we can add new schedule
    if (!isEdit && scheduleCount >= MAX_SCHEDULES) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Maximum schedules reached (6)\"}");
        return;
    }
    
    Schedule newSchedule;
    
    // Fill schedule data
    newSchedule.enabled = doc["enabled"] | true;
    newSchedule.relay = doc["relay"] | 1;
    newSchedule.action = doc["action"] | true;
    newSchedule.startHour = doc["startHour"] | 0;
    newSchedule.startMinute = doc["startMinute"] | 0;
    newSchedule.endHour = doc["endHour"] | 0;
    newSchedule.endMinute = doc["endMinute"] | 0;
    newSchedule.executedToday = false;
    
    // Get days
    JsonArray days = doc["days"];
    for (int i = 0; i < DAYS_IN_WEEK && i < days.size(); i++) {
        newSchedule.days[i] = days[i];
    }
    
    // Validate time
    if (newSchedule.startHour < 0 || newSchedule.startHour > 23 ||
        newSchedule.startMinute < 0 || newSchedule.startMinute > 59 ||
        newSchedule.endHour < 0 || newSchedule.endHour > 23 ||
        newSchedule.endMinute < 0 || newSchedule.endMinute > 59) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid time\"}");
        return;
    }
    
    // Validate at least one day is selected
    bool hasDay = false;
    for (int i = 0; i < DAYS_IN_WEEK; i++) {
        if (newSchedule.days[i]) {
            hasDay = true;
            break;
        }
    }
    
    if (!hasDay) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Select at least one day\"}");
        return;
    }
    
    if (isEdit) {
        // Update existing schedule
        schedules[index] = newSchedule;
    } else {
        // Add new schedule
        schedules[scheduleCount] = newSchedule;
        scheduleCount++;
    }
    
    // Save to EEPROM
    saveSchedules();
    
    // Print schedule for debugging
    printSchedule(isEdit ? index : scheduleCount - 1);
    
    String response = "{";
    response += "\"status\":\"success\",";
    response += "\"message\":\"Schedule " + String(isEdit ? "updated" : "saved") + " successfully\",";
    response += "\"count\":" + String(scheduleCount);
    response += "}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleDeleteSchedule() {
    String body = server.arg("plain");
    
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid JSON\"}");
        return;
    }
    
    int index = doc["index"];
    
    if (index < 0 || index >= scheduleCount) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid schedule index\"}");
        return;
    }
    
    // Shift schedules down
    for (int i = index; i < scheduleCount - 1; i++) {
        schedules[i] = schedules[i + 1];
    }
    
    scheduleCount--;
    
    // Clear last schedule
    memset(&schedules[scheduleCount], 0, sizeof(Schedule));
    
    // Save to EEPROM
    saveSchedules();
    
    String response = "{";
    response += "\"status\":\"success\",";
    response += "\"message\":\"Schedule deleted successfully\",";
    response += "\"count\":" + String(scheduleCount);
    response += "}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
}

void handleGetSchedule() {
    if (!server.hasArg("index")) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No index specified\"}");
        return;
    }
    
    int index = server.arg("index").toInt();
    
    if (index < 0 || index >= scheduleCount) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid schedule index\"}");
        return;
    }
    
    Schedule& sched = schedules[index];
    
    String json = "{";
    json += "\"status\":\"success\",";
    json += "\"index\":" + String(index) + ",";
    json += "\"enabled\":" + String(sched.enabled ? "true" : "false") + ",";
    json += "\"relay\":" + String(sched.relay) + ",";
    json += "\"action\":" + String(sched.action ? "true" : "false") + ",";
    json += "\"startHour\":" + String(sched.startHour) + ",";
    json += "\"startMinute\":" + String(sched.startMinute) + ",";
    json += "\"endHour\":" + String(sched.endHour) + ",";
    json += "\"endMinute\":" + String(sched.endMinute) + ",";
    json += "\"days\":[";
    
    for (int d = 0; d < DAYS_IN_WEEK; d++) {
        json += String(sched.days[d] ? "true" : "false");
        if (d < DAYS_IN_WEEK - 1) json += ",";
    }
    
    json += "]}";
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", json);
}

void handleNotFound() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(404, "text/plain", "Endpoint not found");
}
