#include <Wire.h>
#include <RTClib.h>
#include <TCA9548A.h>
#include <PZEM004Tv30.h>
#include <LiquidCrystal_I2C.h>

// Create objects
TCA9548A tca;
RTC_DS3231 rtc;
HardwareSerial PZEMSerial(1); // Use hardware serial port 1
PZEM004Tv30 pzem(&PZEMSerial, 16, 17); // PZEM on RX=16, TX=17
LiquidCrystal_I2C lcd(0x27, 20, 4); // 20x4 LCD with I2C

// Define pins
#define HALL_SENSOR_PIN 4
#define RELAY_PIN 5
#define EEPROM_ADDR 0x50

// Function prototypes
void selectI2CChannel(uint8_t channel);
void writeStringToEEPROM(int address, String data);
void displayOnLCD(int cycle);

// Global memory address for EEPROM writes
int memoryAddress = 0;
int displayCycle = 0;

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);

  // Initialize I2C communication
  Wire.begin();

  // Initialize Hall Effect Sensor and Relay
  pinMode(HALL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Set relay to NC (default state)
  Serial.println("Hall Effect Sensor and Relay Initialized");

  // Initialize TCA9548A multiplexer
  tca.begin(Wire);

  // Initialize RTC on channel 2
  selectI2CChannel(2);
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC DS3231. Check connections!");
    while (1);
  }

  // Initialize PZEM module
  float voltage = pzem.voltage();
  if (isnan(voltage)) {
    Serial.println("Error: PZEM module not detected! Check connections.");
    while (1);
  }
  Serial.println("PZEM module detected successfully!");

  // Initialize LCD on channel 7
  selectI2CChannel(7);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("System Initialized!");

  Serial.println("Setup Complete");
  delay(2000);
}

void loop() {
  // Select channel 2 for RTC
  selectI2CChannel(2);

  // Fetch current time from RTC
  DateTime now = rtc.now();
  String timestamp = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year()) +
                     " " + String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());

  // Select channel 4 for EEPROM
  selectI2CChannel(4);

  // Read Hall Effect Sensor
  int sensorValue = digitalRead(HALL_SENSOR_PIN);
  String hallStatus = (sensorValue == HIGH) ? "No magnetic field (HIGH)" : "Magnetic field detected (LOW)";

  // Control relay based on Hall Effect Sensor status
  if (sensorValue == LOW) {
    digitalWrite(RELAY_PIN, HIGH); // Switch relay GPIO to HIGH when magnetic field is detected
  } else {
    digitalWrite(RELAY_PIN, LOW);  // Default relay GPIO to LOW when no magnetic field
  }

  // Read PZEM values
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();

  // Handle NaN values
  if (isnan(voltage) || isnan(current) || isnan(power) || isnan(energy)) {
    Serial.println("Error reading PZEM data!");
    return;
  }

  // Combine data into a string
  String dataToWrite = "Timestamp: " + timestamp + " | Voltage: " + String(voltage, 2) + " V | Current: " +
                       String(current, 2) + " A | Power: " + String(power, 2) + " W | Energy: " +
                       String(energy, 2) + " kWh | Hall Status: " + hallStatus;

  // Print data to Serial Monitor
  Serial.println(dataToWrite);

  // Write data to EEPROM
  writeStringToEEPROM(memoryAddress, dataToWrite);

  // Confirm data written
  Serial.println("Data successfully written to EEPROM!");

  // Update memory address for the next write
  memoryAddress += dataToWrite.length() + 1; // Account for null terminator

  // Display data on LCD cyclically
  selectI2CChannel(7);
  displayOnLCD(displayCycle);
  displayCycle = (displayCycle + 1) % 4; // Rotate display cycles

  delay(2000); // Update every 2 seconds
}

// Function to select a specific I2C channel on TCA9548A
void selectI2CChannel(uint8_t channel) {
  if (channel > 7) {
    Serial.println("Invalid TCA9548A channel!");
    return;
  }
  tca.openChannel(channel); // Open the specified channel
}

// Function to write a string to the EEPROM
void writeStringToEEPROM(int address, String data) {
  for (int i = 0; i < data.length(); i++) {
    Wire.beginTransmission(EEPROM_ADDR);
    Wire.write((int)((address + i) >> 8));   // High byte of the address
    Wire.write((int)((address + i) & 0xFF)); // Low byte of the address
    Wire.write(data[i]);  // Write each character
    Wire.endTransmission();
    delay(5);  // Ensure EEPROM completes writing
  }

  // Add a null terminator at the end
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write((int)((address + data.length()) >> 8));   // Address for null terminator
  Wire.write((int)((address + data.length()) & 0xFF)); // Address for null terminator
  Wire.write(0x00);  // Write null terminator
  Wire.endTransmission();
  delay(5);
}

// Function to display data cyclically on LCD
void displayOnLCD(int cycle) {
  lcd.clear();
  switch (cycle) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Voltage: " + String(pzem.voltage(), 2) + " V");
      lcd.setCursor(0, 1);
      lcd.print("Current: " + String(pzem.current(), 2) + " A");
      break;
    case 1:
      lcd.setCursor(0, 0);
      lcd.print("Power: " + String(pzem.power(), 2) + " W");
      lcd.setCursor(0, 1);
      lcd.print("Energy: " + String(pzem.energy(), 2) + " kWh");
      break;
    case 2:
      lcd.setCursor(0, 0);
      lcd.print("Hall Status:");
      lcd.setCursor(0, 1);
      lcd.print((digitalRead(HALL_SENSOR_PIN) == HIGH) ? "No Field" : "Magnetic Field");
      break;
    case 3:
      lcd.setCursor(0, 0);
      lcd.print("Last Update:");
      lcd.setCursor(0, 1);
      lcd.print(rtc.now().timestamp().c_str());
      break;
  }
}
