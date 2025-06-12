#include <Keypad.h>  // Include the Libraries for KeyPad
#include <LiquidCrystal.h> // LCD Library
//#include <SoftwareSerial.h> // Library for Serial communication

// Define the LCD pins
LiquidCrystal lcd(10, 11, 12, 13, A2, A3); // RS, E, D4, D5, D6, D7

// Define the analog input pins for CT (Current Transformer) and PT (Potential Transformer)
const int ctPin = A1; // Analog pin for current sensor
const int ptPin = A0; // Analog pin for voltage sensor

// Relay control pin
const int relayPin = 9; // Relay connected to digital pin 9



// Keypad configuration
const byte ROWS = 4; // Four rows
const byte COLS = 3; // Three columns

// Map of Keys
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};

byte rowPins[ROWS] = {2, 3, 4, 5}; // Rows connected to pins 2, 3, 4, 5
byte colPins[COLS] = {6, 7, 8};    // Columns connected to pins 6, 7, 9

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Serial configuration using software Serial
//SoftwareSerial Serial(0, 1); // Serial RX on pin 0, TX on pin 1

// Energy and balance variables
float accountBalance = 15.0;  // Initial balance in Rs
float energyConsumed = 0.0;   // Total energy consumed in watt-hours
float costPerUnit = 1.0;      // Cost per unit (1 Rs per unit)
float wattsPerUnit = 100.0;   // 1 unit = 100 watts
float power;                  // Power in watts
float unitsConsumed = 0.0;    // Number of units consumed
unsigned long previousMillis = 0;
const unsigned long interval = 1000;  // Update every 1 second

// Recharge function
void rechargeBalance() {
  lcd.clear();
  lcd.print("Enter recharge:");
  String enteredValue = ""; // To store the entered recharge value
  char key;

  while (true) {
    key = keypad.getKey();
    if (key) {
      if (key == '#') { // Complete the entry on '#'
        accountBalance += enteredValue.toFloat(); // Update the balance
        lcd.clear();
        lcd.print("Recharged: Rs ");
        lcd.print(enteredValue);
        delay(1000);
        break;
      } else {
        enteredValue += key; // Append the entered key to the recharge value
        lcd.setCursor(0, 1);
        lcd.print(enteredValue);
      }
    }
  }
}

void sendDataToServer(float voltage, float current, float unitsConsumed, float accountBalance) {
  Serial.println("AT+CMGF=1");  // Set SMS mode to text
  delay(500);
  Serial.println("AT+CMGS=\"+8308213536\"");  // Replace with your phone number
  delay(500);
  Serial.print("Energy Meter Data:\n");
  Serial.print("Voltage: "); Serial.print(voltage); Serial.println(" V");
  Serial.print("Current: "); Serial.print(current); Serial.println(" A");
  Serial.print("Units: "); Serial.print(unitsConsumed); Serial.println(" kWh");
  Serial.print("Balance: "); Serial.print(accountBalance); Serial.println(" Rs");
  Serial.write(26);  // Ctrl+Z to send SMS
  delay(500);
}

void setup() {
  // Initialize the LCD
  lcd.begin(16, 2);  // 16 columns, 2 rows
  lcd.print("Automated Energy");
  lcd.setCursor(0, 1);
  lcd.print("Metering System");

  // Initialize the relay pin
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);  // Relay ON initially (connected)

  // Serial setup
  Serial.begin(9600);  // Set baud rate for Serial module
  delay(500);
  Serial.println("AT");  // Check if the Serial module is working
  delay(500);
}

void loop() {
  // Get current time for timing control
  unsigned long currentMillis = millis();

  // Only update once per interval (1 second)
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // 1. *Read real-time voltage and current*
    float current = readCurrent();   // Current from CT
    float voltage = readVoltage();   // Voltage from PT
    power = current * voltage;       // Power = Voltage × Current (in watts)

    // 2. *Accumulate energy consumption*
    energyConsumed += power * (interval / 3600.0);  // Convert power from watts to watt-hours

    // 3. *Deduct balance based on energy consumption*
    if (energyConsumed >= wattsPerUnit) {
      unitsConsumed += energyConsumed / wattsPerUnit;  // Increment units consumed
      accountBalance -= (energyConsumed / wattsPerUnit) * costPerUnit;  // Deduct cost per unit
      energyConsumed = 0;  // Reset energy after reducing balance
    }

    // 4. *Display voltage, current, power, units, and balance on LCD*
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("V: ");
    lcd.print(voltage, 1);  // Display voltage
    lcd.print(" I: ");
    lcd.print(current, 1);  // Display current

    lcd.setCursor(0, 1);
    lcd.print("Units: ");
    lcd.print(unitsConsumed, 1);  // Display units consumed
    lcd.print(" Bal: ");
    lcd.print(accountBalance, 2);  // Display balance in Rs

    // Send data to Serial
    sendDataToServer(voltage, current, unitsConsumed, accountBalance);

    // 5. *Relay control based on balance*
    if (accountBalance <= 0) {
      lcd.clear();
      lcd.print("Balance: 0 Rs");
      lcd.setCursor(0, 1);
      lcd.print("Recharge needed!");

      digitalWrite(relayPin, LOW);  // Turn off relay

      // Recharge when balance is zero
      rechargeBalance();

      // After recharging, reset the relay to ON
      digitalWrite(relayPin, HIGH);
    }

    // Optional delay for readability on LCD
    delay(500);
  }
}

// Read Current from CT
float readCurrent() {
  int sensorValue = analogRead(ctPin);  // Read the CT sensor value
  float current = (sensorValue / 1023.0) * 5.0;  // Convert analog value to current
  return current;
}

// Read Voltage from PT
float readVoltage() {
  int sensorValue = analogRead(ptPin);  // Read the PT sensor value
  float voltage = (sensorValue / 1023.0) * 230.0;  // Convert analog value to voltage (assuming 0-230V)
  return voltage;
}
