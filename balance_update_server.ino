#include <Wire.h>
#include <RTClib.h>
#include <TCA9548A.h>
#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi Credentials and Server Details
const char* ssid = "CAPL_1";  // Replace with your WiFi SSID
const char* password = "Cimer_3210";  // Replace with your WiFi password
const char* serverURL = "http://192.168.1.11:3000/api/consumption";  // Replace with your server URL

// Create objects
TCA9548A tca;
RTC_DS3231 rtc;
HardwareSerial PZEMSerial(1); // Use hardware serial port 1
PZEM004Tv30 pzem(&PZEMSerial, 16, 17); // PZEM on RX=16, TX=17
LiquidCrystal_I2C lcd(0x27, 20, 4); // 20x4 LCD with I2C

// Define pins
#define HALL_SENSOR_PIN 4
#define RELAY_PIN 5
#define TAMPER_BUTTON_PIN 23
#define EEPROM_ADDR 0x50

// Global variables
float balance = 10.0; // Initialize balance to 10 RS
int memoryAddress = 0;
int displayCycle = 0;
unsigned long lastDeductionTime = 0;
String mac_Id = "MAC00123E"; // Unique MAC ID
String currentTimestamp; // Global timestamp for consistency

// Function prototypes
void selectI2CChannel(uint8_t channel);
void writeStringToEEPROM(int address, String data);
void displayOnLCD(int cycle);
void handleTamper();
void connectToWiFi();
void sendDataToServer(String payload);
void deductBalance(float power, unsigned long duration);
String getFormattedTimestamp();

void setup() {
    Serial.begin(115200);
    delay(500);

    // Initialize I2C communication
    Wire.begin();

    // Initialize TCA9548A multiplexer
    tca.begin(Wire);

    // Initialize RTC on channel 2
    selectI2CChannel(2);
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC DS3231. Check connections!");
        while (1);
    }

    // Uncomment this line once to set the RTC to the current date and time
     rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    // Initialize Hall Effect Sensor, Relay, and Tamper Button
    pinMode(HALL_SENSOR_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(TAMPER_BUTTON_PIN, INPUT_PULLUP);
    digitalWrite(RELAY_PIN, LOW); // Default relay state

    // Initialize PZEM module
    float voltage = pzem.voltage();
    if (isnan(voltage)) {
        Serial.println("Error: PZEM module not detected! Check connections.");
        while (1);
    }

    // Initialize LCD on channel 7
    selectI2CChannel(7);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Initialized!");

    // Connect to WiFi
    connectToWiFi();

    Serial.println("Setup Complete");
    delay(500);
}

void loop() {
    // Capture timestamp at the start of the loop
    currentTimestamp = getFormattedTimestamp();

    // Check for tamper condition
    if (digitalRead(TAMPER_BUTTON_PIN) == HIGH) {
        handleTamper();
        return; // Skip normal operation if tampered
    }

    // Read Hall Effect Sensor
    int sensorValue = digitalRead(HALL_SENSOR_PIN);
    String hallStatus = (sensorValue == HIGH) ? "No magnetic field (HIGH)" : "Magnetic field detected (LOW)";

    // Control relay based on Hall Sensor status
    digitalWrite(RELAY_PIN, (sensorValue == LOW) ? HIGH : LOW);

    // Read PZEM values
    float voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();

    if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy)) {
        Serial.println("Error reading PZEM data!");
        return;
    }

    // Deduct balance based on power usage
    unsigned long currentMillis = millis();
    if (currentMillis - lastDeductionTime >= 60000) { // Deduct every minute
        deductBalance(power, (currentMillis - lastDeductionTime) / 60000);
        lastDeductionTime = currentMillis;
    }

    // Prepare data to send to server
    String payload = "{\"mac_Id\": \"" + mac_Id + "\", \"time_stamp\": \"" + currentTimestamp +
                     "\", \"consumption\": " + String(energy, 2) +
                     ", \"relay_status\": \"" + (sensorValue == LOW ? "ON" : "OFF") +
                     "\", \"voltage\": " + String(voltage, 2) +
                     ", \"current\": " + String(current, 2) +
                     ", \"power\": " + String(power, 2) +
                     ", \"balance\": " + String(balance, 2) +
                     ", \"power_factor\": 0.95, \"frequency\": 50, " +
                     "\"reset_time\": \"2024-11-25T12:00:00Z\", \"update_time\": \"" + currentTimestamp + "\"}";

    // Send data to server
    sendDataToServer(payload);

    // Display data on LCD cyclically
    selectI2CChannel(7);
    displayOnLCD(displayCycle);
    displayCycle = (displayCycle + 1) % 4;

    delay(2000); // Update every 2 seconds
}

void deductBalance(float power, unsigned long duration) {
    float deduction = (power / 10.0) * duration; // 10 RS for 10W per minute
    balance -= deduction;
    if (balance < 0) {
        balance = 0;
        digitalWrite(RELAY_PIN, HIGH); // Turn off relay if balance is zero
        Serial.println("Balance exhausted! Relay turned off.");
    }
    Serial.println("Balance Deducted: " + String(deduction, 2) + " RS, Remaining Balance: " + String(balance, 2) + " RS");
}

void sendDataToServer(String payload) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Server Response: " + response);

        // Parse the response
        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);

        float serverBalance = doc["balance"];
        balance = serverBalance; // Update balance based on server response

        Serial.println("Updated Balance: " + String(balance, 2));
    } else {
        Serial.println("Failed to send data to server. Response code: " + String(httpResponseCode));
    }

    http.end();
}

void handleTamper() {
    Serial.println("Tamper detected! Switching to tampered state.");
    digitalWrite(RELAY_PIN, HIGH);

    selectI2CChannel(7);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("System Tampered!");
    lcd.setCursor(0, 1);
    lcd.print("Check System!");

    selectI2CChannel(2);
    String tamperData = "Tamper detected at: " + currentTimestamp;
    writeStringToEEPROM(memoryAddress, tamperData);
    memoryAddress += tamperData.length() + 1;

    while (digitalRead(TAMPER_BUTTON_PIN) == LOW) {
        delay(100);
    }
    Serial.println("Tamper state cleared.");
}

void selectI2CChannel(uint8_t channel) {
    if (channel > 7) {
        Serial.println("Invalid TCA9548A channel!");
        return;
    }
    tca.openChannel(channel);
}

void writeStringToEEPROM(int address, String data) {
    for (int i = 0; i < data.length(); i++) {
        Wire.beginTransmission(EEPROM_ADDR);
        Wire.write((int)((address + i) >> 8));
        Wire.write((int)((address + i) & 0xFF));
        Wire.write(data[i]);
        Wire.endTransmission();
        delay(5);
    }

    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write((int)((address + data.length()) >> 8));
    Wire.write((int)((address + data.length()) & 0xFF));
    Wire.write(0x00);
    Wire.endTransmission();
    delay(5);
}

void displayOnLCD(int cycle) {
    lcd.clear();
    switch (cycle) {
        case 0:
            lcd.setCursor(0, 0);
            lcd.print("Date: ");
            lcd.print(currentTimestamp);
            lcd.setCursor(0, 1);
            lcd.print("Voltage: ");
            lcd.print(pzem.voltage(), 2);
            lcd.print("V");
            break;
        case 1:
            lcd.setCursor(0, 0);
            lcd.print("Date: ");
            lcd.print(currentTimestamp);
            lcd.setCursor(0, 1);
            lcd.print("Current: ");
            lcd.print(pzem.current(), 2);
            lcd.print("A");
            break;
        case 2:
            lcd.setCursor(0, 0);
            lcd.print("Date: ");
            lcd.print(currentTimestamp);
            lcd.setCursor(0, 1);
            lcd.print("Power: ");
            lcd.print(pzem.power(), 2);
            lcd.print("W");
            break;
        case 3:
            lcd.setCursor(0, 0);
            lcd.print("Date: ");
            lcd.print(currentTimestamp);
            lcd.setCursor(0, 1);
            lcd.print("Balance: ");
            lcd.print(balance, 2);
            lcd.print("RS");
            break;
    }
}

void connectToWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected to WiFi!");
}

String getFormattedTimestamp() {
    DateTime now = rtc.now();
    return String(now.year()) + "-" + (now.month() < 10 ? "0" : "") + String(now.month()) + "-" +
           (now.day() < 10 ? "0" : "") + String(now.day()) + " " +
           (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" +
           (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" +
           (now.second() < 10 ? "0" : "") + String(now.second());
}
