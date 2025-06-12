#include <Wire.h>
#include <RTClib.h>
#include <TCA9548A.h>
#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ZMPT101B.h>
#include <Preferences.h>  
#include <SoftwareSerial.h>
#include <SPIFFS.h>
#define I2C_MUX_ADDR 0x70

SoftwareSerial sim800lSerial(3, 1);
const String PHONE_NUMBER = "+918308213536";
unsigned long previousMillis = 0;
const long interval = 10000; 

const char* ssid = "CAPL_1";
const char* password = "Cimer_3210"; 
const char* serverURL = "http://116.74.138.184:3000/api/consumption"; 
const char* alertURL = "http://116.74.138.184:3000/api/alert";

TCA9548A tca;
RTC_DS3231 rtc;
HardwareSerial PZEMSerial(1); 
PZEM004Tv30 pzem(&PZEMSerial, 16, 17); 
LiquidCrystal_I2C lcd(0x27, 20, 4); 
ZMPT101B zmptNeutralSensor(33, 50.0);
Preferences preferences;  

#define HALL_SENSOR_PIN 4
#define RELAY_PIN 5
#define TAMPER_BUTTON_PIN 23


float balance = 10.0; 
int displayCycle = 0;
unsigned long lastDeductionTime = 0;
String mac_Id = "MAC00123E"; 
String user_Id = "USR004";
String currentTimestamp; 
const float LOW_BALANCE_THRESHOLD = 5.0; 
const float ZERO_BALANCE_THRESHOLD = 0.0; 
bool lowBalanceAlertSent = false; 
bool zeroBalanceAlertSent = false; 
bool tamperAlertSent = false; 
unsigned long lastDataUpdateTime = 0; 

String relayStatus = "ON"; 

const long utcOffsetInSeconds = 5 * 3600 + 30 * 60; 
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
    Serial.begin(115200);
    preferences.begin("energy-meter", false); 

    balance = loadBalanceFromNVS();  
    float energy = loadConsumptionFromNVS();  
    preferences.end(); 

    Wire.begin();

    tca.begin(Wire);

    selectI2CChannel(2);
    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC DS3231. Check connections!");
        while (1);
    }

    pinMode(HALL_SENSOR_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(TAMPER_BUTTON_PIN, INPUT_PULLUP);

    float voltage = pzem.voltage();
    if (isnan(voltage)) {
        Serial.println("Error: PZEM module not detected! Check connections.");
        while (1);
    }

    zmptNeutralSensor.setSensitivity(156.5f);

    selectI2CChannel(7);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Initialized!");
  
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    connectToWiFi();
    syncRTCWithNTP();

    timeClient.begin();
    timeClient.update();
    if (WiFi.status() == WL_CONNECTED) {
        rtc.adjust(DateTime(timeClient.getEpochTime()));
    }

    sim800lSerial.begin(9600);
    delay(1000);
    sendATCommand("AT", 1000); 
    sendATCommand("AT+CMGF=1", 1000); 
    sendATCommand("AT+CNMI=1,2,0,0,0", 1000); 

    Serial.println("Setup Complete");
}

void sendATCommand(String cmd, unsigned long timeout) {
    Serial.print("Sending: ");
    Serial.println(cmd);
    sim800lSerial.println(cmd);
    delay(timeout);
}

void sendSMS(String message) {
    sendATCommand("AT+CMGS=\"" + PHONE_NUMBER + "\"", 1000);
    sim800lSerial.print(message);
    sim800lSerial.write(26); 
    delay(1000);
}
void syncRTCWithNTP() {
    timeClient.begin();
    timeClient.update();
    if (WiFi.status() == WL_CONNECTED) {
        rtc.adjust(DateTime(timeClient.getEpochTime()));  
        Serial.println("RTC Synced with NTP.");
    } else {
        Serial.println("Failed to sync RTC. No WiFi.");
    }
}
void loop() {
    currentTimestamp = getFormattedTimestamp();
     
    bool isTampered = false;
    if (digitalRead(TAMPER_BUTTON_PIN) == HIGH) {
        isTampered = true;
    }

    if (digitalRead(HALL_SENSOR_PIN) == LOW) {
        isTampered = true;
    }

    float voltage = pzem.voltage();
    float neutralVoltage = zmptNeutralSensor.getRmsVoltage();
    float voltageDifference = abs(voltage - neutralVoltage);
    if (voltageDifference > 300.0) {
        isTampered = true;
    }

    if (isTampered && !tamperAlertSent) {
        if(digitalRead(TAMPER_BUTTON_PIN) == HIGH){
        sendTamperingAlertToServer("Tampering detected! Relay turned off. Due to Button Pressed!!!");
        tamperAlertSent = true;
        } else if(digitalRead(HALL_SENSOR_PIN) == LOW){
        sendTamperingAlertToServer("Tampering detected! Relay turned off. Due to Magnetic Field detected!!!");
        tamperAlertSent = true;
        }else if (voltageDifference > 300.0) {
        sendTamperingAlertToServer("Tampering detected! Relay turned off. Due to Voltage Tampering!!!");
        tamperAlertSent = true;
        }

    }

    if (!isTampered) {
        tamperAlertSent = false;
    }

    checkBalanceAlerts();

    if (isTampered || balance <= ZERO_BALANCE_THRESHOLD || relayStatus == "OFF") {
        relayStatus = "OFF";
    }
    digitalWrite(RELAY_PIN, (relayStatus == "OFF") ? HIGH : LOW);

    voltage = pzem.voltage();
    float current = pzem.current();
    float power = pzem.power();
    float energy = pzem.energy();

    float consumption = (power * 10)/(3600000);
    Serial.print("Consumption: "+ String(consumption, 6));

    if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy)) {
        Serial.println("Error reading PZEM data!");
        return;
    }

    unsigned long currentMillis = millis();
    if (currentMillis - lastDeductionTime >= 60000) { 
        deductBalance(power, (currentMillis - lastDeductionTime) / 60000);
        lastDeductionTime = currentMillis;
    }

    while (sim800lSerial.available()) {
        Serial.write(sim800lSerial.read());
    }

    String payload = "{\"mac_Id\": \"" + mac_Id + "\", \"time_stamp\": \"" + currentTimestamp +
                     "\", \"consumption\": " + String(consumption, 6) +
                     ", \"relay_status\": \"" + relayStatus +
                     "\", \"voltage\": " + String(voltage, 2) +
                     ", \"current\": " + String(current, 2) +
                     ", \"power\": " + String(power, 2) +
                     ", \"balance\": " + String(balance, 2) +
                     ", \"power_factor\": 0.95, \"frequency\": 50, " +
                     "\"reset_time\": \"2024-11-25T12:00:00Z\", \"update_time\": \"" + currentTimestamp + "\"" +
                     ", \"tamper_detected\": " + 
                     (isTampered ? "true" : "false") +
                     "}";

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        if (WiFi.status() == WL_CONNECTED && serverAvailable()) {
            sendDataToServer(payload);
            resendStoredData(); 
        } else {
            storeDataLocally(payload); 
        }

        Serial.println("Current Timestamp: " + currentTimestamp);
    }

    
    displayOnLCD(displayCycle);
    displayCycle = (displayCycle + 1) % 4;

    delay(10000);
}

void deductBalance(float power, unsigned long duration) {
   
    float deduction = (power / 10.0) * duration;
    balance -= deduction;
    if (balance < 0) {
        balance = 0;
        digitalWrite(RELAY_PIN, HIGH);
        relayStatus = "OFF";
        Serial.println("Balance exhausted! Relay turned off.");
    }
    Serial.println("Balance Deducted: " + String(deduction, 2) + " RS, Remaining Balance: " + String(balance, 2) + " RS");
    saveBalanceToNVS(balance);
}
void saveBalanceToNVS(float balance) {
    preferences.begin("energy-meter", false);  
    preferences.putFloat("balance", balance);  
    preferences.end();  
}

float loadBalanceFromNVS() {
    preferences.begin("energy-meter", true);  
    float balance = preferences.getFloat("balance", 10.0);  
    preferences.end();  
    return balance;
}

void saveConsumptionToNVS(float energy) {
    preferences.begin("energy-meter", false);  
    preferences.putFloat("consumption", energy); 
    preferences.end();  
}

float loadConsumptionFromNVS() {
    preferences.begin("energy-meter", true);  
    float energy = preferences.getFloat("consumption", 0.0);  
    preferences.end();  
    return energy;
}

void checkBalanceAlerts() {
    
    if (balance <= LOW_BALANCE_THRESHOLD && balance > ZERO_BALANCE_THRESHOLD) {
        if (!lowBalanceAlertSent) {
            sendBalanceAlert("Balance Low: Recharge Soon!");
            lowBalanceAlertSent = true; 
        }
    } else if (balance <= ZERO_BALANCE_THRESHOLD) {
        if (!zeroBalanceAlertSent) {
            sendBalanceAlert("Balance Zero: Service Disabled. Please Recharge!");
            zeroBalanceAlertSent = true; 
        }
    }

    if (balance > LOW_BALANCE_THRESHOLD) {
        lowBalanceAlertSent = false;
        zeroBalanceAlertSent = false;
    }
}

void sendBalanceAlert(String alertMessage) {
    String time_stamp = getFormattedTimestamp();
    String alertJson = "{";
    alertJson += "\"mac_Id\":\"" + mac_Id + "\",";
    alertJson += "\"user_Id\":\"" + user_Id + "\",";
    alertJson += "\"alert\":\"" + alertMessage + "\",";
    alertJson += "\"time_stamp\":\"" + time_stamp + "\"";
    alertJson += "}";

    if (WiFi.status() == WL_CONNECTED && serverAvailable()) {
        HTTPClient http;
        http.begin(alertURL);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(alertJson);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Alert Sent: " + response);
            sendSMS("ALERT: " + alertMessage); 
        } else {
            Serial.println("Failed to send alert. Response code: " + String(httpResponseCode));
            storeAlertLocally(alertJson); 
        }

        http.end();
    } else {
        storeAlertLocally(alertJson); 
    }
}

void sendTamperingAlertToServer(String alertMessage) {
    String time_stamp = getFormattedTimestamp();
    String alertJson = "{";
    alertJson += "\"mac_Id\":\"" + mac_Id + "\",";
    alertJson += "\"user_Id\":\"" + user_Id + "\",";
    alertJson += "\"alert\":\"" + alertMessage + "\",";
    alertJson += "\"time_stamp\":\"" + time_stamp + "\"";
    alertJson += "}";

    if (WiFi.status() == WL_CONNECTED && serverAvailable()) {
        // Send alert to server
        HTTPClient http;
        http.begin(alertURL);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.POST(alertJson);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Tampering Alert Sent: " + response);
            sendSMS("TAMPER ALERT: " + alertMessage); 
        } else {
            Serial.println("Failed to send tampering alert. Response code: " + String(httpResponseCode));
            storeAlertLocally(alertJson); 
        }

      http.end();
    } else {
        storeAlertLocally(alertJson);
        Serial.println("Wi-Fi unavailable. Tampering alert stored locally.");
    }

    
}

void storeAlertLocally(String alertJson) {
    Serial.println("No connection, storing alert locally...");
    File file = SPIFFS.open("/alerts.txt", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for writing alerts");
        return;
    }
    file.println(alertJson);
    file.close();
    Serial.println("Alert stored successfully.");
}
void resendStoredAlerts() {
    File file = SPIFFS.open("/alerts.txt", FILE_READ);
    if (!file) {
        Serial.println("No stored alerts found.");
        return;
    }

    Serial.println("Resending stored alerts...");
    while (file.available()) {
        String storedAlert = file.readStringUntil('\n');
        sendAlertToServer(storedAlert);
    }
    file.close();
    SPIFFS.remove("/alerts.txt"); 
}

void sendAlertToServer(String alertJson) {
    HTTPClient http;
    http.begin(alertURL);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(alertJson);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Alert Sent: " + response);
    } else {
        Serial.println("Failed to send alert. Response code: " + String(httpResponseCode));
    }

    http.end();
}
void sendDataToServer(String payload) {
    HTTPClient http;
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("Server Response: " + response);

        DynamicJsonDocument doc(1024);
        deserializeJson(doc, response);

        if (doc.containsKey("balance")) {
            balance = doc["balance"];
        } else {
            Serial.println("Balance not found in server response. Using current balance.");
        }

        if (doc.containsKey("relay_status")) {
            String newRelayStatus = doc["relay_status"].as<String>();
            if (newRelayStatus == "ON" || newRelayStatus == "OFF") {
                relayStatus = newRelayStatus;
            }
        } else {
            Serial.println("Relay status not found in server response. Using current status.");
        }

        Serial.println("Updated Balance: " + String(balance, 2));
        Serial.println("Updated Relay Status: " + relayStatus);
    } else {
        Serial.println("Failed to send data to server. Response code: " + String(httpResponseCode));
        Serial.println("Storing data in SPIFFS...");
        storeDataLocally(payload);
    }

    http.end();
}
void storeDataLocally(String data) {
    Serial.println("No connection, storing data locally...");
    File file = SPIFFS.open("/data.txt", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.println(data);
    file.close();
    Serial.println("Data stored successfully.");
}

void selectI2CChannel(uint8_t channel) {
    if (channel > 7) {
        Serial.println("Invalid TCA9548A channel!");
        return;
    }
    tca.openChannel(channel);
}
void resendStoredData() {
  
    File dataFile = SPIFFS.open("/data.txt", FILE_READ);
    if (dataFile) {
        Serial.println("Resending stored data...");
        while (dataFile.available()) {
            String storedData = dataFile.readStringUntil('\n');
            sendDataToServer(storedData);
        }
        dataFile.close();
        SPIFFS.remove("/data.txt");  
    } else {
        Serial.println("No stored data found.");
    }

    
    resendStoredAlerts();
}
bool serverAvailable() {
    HTTPClient http;
    http.begin(serverURL);
    int httpCode = http.GET();
    http.end();
    return (httpCode > 0);
}

void displayOnLCD(int cycle) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Timestamp:");
    lcd.setCursor(0, 1);
    lcd.print(currentTimestamp);

    switch (cycle) {
        case 0:
            lcd.setCursor(0, 2);
            lcd.print("Voltage: ");
            lcd.print(pzem.voltage(), 2);
            lcd.print(" V");
            lcd.setCursor(0, 3);
            lcd.print("Current: ");
            lcd.print(pzem.current(), 2);
            lcd.print(" A");
            break;
        case 1:
            lcd.setCursor(0, 2);
            lcd.print("Power: ");
            lcd.print(pzem.power(), 2);
            lcd.print(" W");
            lcd.setCursor(0, 3);
            lcd.print("Energy: ");
            lcd.print(pzem.energy(), 2);
            lcd.print(" kWh");
            break;
        case 2:
            lcd.setCursor(0, 2);
            lcd.print("Balance: ");
            lcd.print(balance, 2);
            lcd.print(" RS");
            lcd.setCursor(0, 3);
            lcd.print("Relay: ");
            lcd.print(relayStatus);
            break;
        case 3:
            lcd.setCursor(0, 2);
            lcd.print("Tamper: ");
            lcd.print((digitalRead(TAMPER_BUTTON_PIN) == HIGH) ? "Button" : "None");
            lcd.setCursor(0, 3);
            lcd.print("V Diff: ");
            lcd.print(abs(pzem.voltage() - zmptNeutralSensor.getRmsVoltage()), 2);
            lcd.print(" V");
            break;
    }
}

String getFormattedTimestamp() {
    DateTime now = rtc.now();
    return String(now.year()) + "-" + (now.month() < 10 ? "0" : "") + String(now.month()) + "-" +
           (now.day() < 10 ? "0" : "") + String(now.day()) + "T" +
           (now.hour() < 10 ? "0" : "") + String(now.hour()) + ":" +
           (now.minute() < 10 ? "0" : "") + String(now.minute()) + ":" +
           (now.second() < 10 ? "0" : "") + String(now.second()) + "Z";
}
void connectToWiFi() {
    Serial.print("Connecting to WiFi");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected to WiFi");
}