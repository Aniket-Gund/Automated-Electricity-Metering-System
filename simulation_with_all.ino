#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <RTClib.h>

// I2C Multiplexer (TCA9548A) Addresses
#define I2C_MUX 0x70
#define LCD_CHANNEL 0
#define RTC_CHANNEL 1
#define EEPROM_CHANNEL 2

// LCD Configuration (20x4)
LiquidCrystal_I2C lcd(0x27, 20, 4);

// RTC Module
RTC_DS1307 rtc;

// Pin Assignments
#define CURRENT_SENSOR A1
#define VOLTAGE_SENSOR A0
#define VOLTAGE_SENSOR_2 A2
#define HALL_SENSOR 22
#define RELAY 9

// Keypad Configuration
const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};
byte rowPins[ROWS] = {2, 3, 4, 5};
byte colPins[COLS] = {6, 7, 8};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Energy & Billing Variables
float voltage, current, power, energy = 0;
float balance = 50.0; // Initial balance
float costPerUnit = 1.0; // Cost per unit (1 Rs per kWh)
float wattsPerUnit = 100.0; // 1 unit = 100 watts
bool relayState = true;
bool magneticTamper = false;
bool voltageTamper = false;
unsigned long previousMillis = 0;
const unsigned long interval = 1000; // Update every 1 second

void setup() {
    Serial.begin(9600);
    Wire.begin();

    // Initialize LCD
    selectI2CChannel(LCD_CHANNEL);
    lcd.begin(20, 4);
    lcd.backlight();
    
    // Initialize RTC
    selectI2CChannel(RTC_CHANNEL);
    if (!rtc.begin()) {
        Serial.println("RTC not found!");
    }
    
    // Set Pins
    pinMode(RELAY, OUTPUT);
    pinMode(HALL_SENSOR, INPUT);
    digitalWrite(RELAY, HIGH); // Relay ON initially

    // Display Welcome Message
    lcd.setCursor(0, 0);
    lcd.print("Smart Metering");
    delay(2000);
    lcd.clear();
}

void loop() {
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;

        readSensors();
        checkTampering();
        updateDisplay();
        manageBilling();
        handleKeypadInput();
    }
}

void readSensors() {
    voltage = analogRead(VOLTAGE_SENSOR) * (230.0 / 1023.0);
    current = analogRead(CURRENT_SENSOR) * (10.0 / 1023.0);
    power = voltage * current;
    energy += power * (interval / 3600.0);
}

void checkTampering() {
    // Magnetic Tampering Detection
    if (digitalRead(HALL_SENSOR) == HIGH) {
        magneticTamper = true;
        digitalWrite(RELAY, LOW);
        lcd.setCursor(0, 1);
        lcd.print("Magnetic Tamper!");
        sendSMS("Magnetic Tampering Detected!");
    } else {
        magneticTamper = false;
        if (!voltageTamper) digitalWrite(RELAY, HIGH);
    }

    // Voltage Tampering Detection
    float voltageDiff = abs(analogRead(VOLTAGE_SENSOR) - analogRead(VOLTAGE_SENSOR_2));
    if (voltageDiff > 10) {
        voltageTamper = true;
        digitalWrite(RELAY, LOW);
        lcd.setCursor(0, 2);
        lcd.print("Voltage Tampered!");
        sendSMS("Voltage Tampering Detected!");
    } else {
        voltageTamper = false;
        if (!magneticTamper) digitalWrite(RELAY, HIGH);
    }
}

void updateDisplay() {
    selectI2CChannel(LCD_CHANNEL);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("V: " + String(voltage) + "V  I: " + String(current) + "A");

    lcd.setCursor(0, 1);
    lcd.print("P: " + String(power) + "W  E: " + String(energy) + "Wh");

    lcd.setCursor(0, 2);
    lcd.print("Balance: " + String(balance));

    lcd.setCursor(0, 3);
    selectI2CChannel(RTC_CHANNEL);
    DateTime now = rtc.now();
    lcd.print("Time: " + String(now.hour()) + ":" + String(now.minute()));
}

void manageBilling() {
    if (energy >= wattsPerUnit) {
        float units = energy / wattsPerUnit;
        balance -= units * costPerUnit;
        energy = 0;
    }

    if (balance <= 0) {
        digitalWrite(RELAY, LOW);
        sendSMS("Balance Low! Please Recharge.");
    }
}

void handleKeypadInput() {
    char key = keypad.getKey();
    if (key) {
        int amount = key - '0';
        balance += amount * 10;
        lcd.setCursor(0, 3);
        lcd.print("Recharged: " + String(amount * 10));
        sendSMS("Recharged: " + String(amount * 10));
    }
}

void sendSMS(String message) {
    Serial.println("AT+CMGF=1");
    delay(500);
    Serial.println("AT+CMGS=\"+918308213536\"");
    delay(500);
    Serial.println(message);
    delay(500);
    Serial.println((char)26);
    delay(5000);
}

void selectI2CChannel(int channel) {
    Wire.beginTransmission(I2C_MUX);
    Wire.write(1 << channel);
    Wire.endTransmission();
}
