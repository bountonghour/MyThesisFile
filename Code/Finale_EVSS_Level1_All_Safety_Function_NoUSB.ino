/*
20th/10/2024

Thesis Name : Design Prototype Eletric Vehicle Supply System Level 1 Using Arduino

========= when no source to arduino=================

This code written by Eletronic Student Of National Polytechnic Institute of Cambodia (NPIC). 
EcE Y4B G16

We do this project as our thesis of Year 4th (Final Year) 

1. BOUN Tonghour (Research, Circuit & Hardware Design, Write Book, Programming, Planing Project)
2. SVAY Puthearoath (Research, Circuit & Hardware Design, Write Book, Planing Project)
3. Ry Sathya (3D Design, Hardware Design, Write Book, Planing Project)

================================================================
Example of using one PZEM module with Software Serial interface.


If only RX and TX pins are passed to the constructor, software 
serial interface will be used for communication with the module.

*/

#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include "Wire.h"
#include <EEPROM.h>

// Define the pin to read
#define CP_IN A0
#define CP_OUT 3
#define READY_Charge_LED 10
#define Fault_LED 9
#define Power_LED 7
#define BUZZER 4
#define POWER_RELAY 11
#define EARTH_PIN 5
#define Fault_PIN 2
#define set_Current_PIN 6
#define PEAK_VOLTAGE_THRESHOLD 1000

// Define the variable
unsigned long Oscilate = pulseIn(EARTH_PIN, HIGH, 50000);
unsigned long startTime = 0;
unsigned long elapsedTime = 0;
bool timerRunning = false;
int peak_voltage = 0;
int amp = 0;
unsigned long wait = 60000;

//threshold for each charging state
int stateA_Thres_min = 1011, stateA_Thres_max = 1023;
int stateB_Thres_min = 995, stateB_Thres_max = 1010;
int stateC_Thres_min = 875, stateC_Thres_max = 990;

//state for condition
bool buzzerActivated = false;
bool undervoltageCondition = false;  // State for undervoltage condition
bool overvoltageCondition = false;   // State for overvoltage condition
bool overCurrentCondition = false;   // State for overCurrent condition

#if defined(ESP32)
#error "Software Serial is not supported on the ESP32"
#endif

/* Use software serial for the PZEM
 * Pin 9 Rx (Connects to the Tx pin on the PZEM)
 * Pin 8 Tx (Connects to the Rx pin on the PZEM)
*/
#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN 13
#define PZEM_TX_PIN 12
#endif

SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzem(pzemSWSerial);

// LCD Display
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  Serial.begin(9600);
  amp = 9;                                    //initial ampear value;
  TCCR2B = TCCR2B & 0b11110000 | 0b00001011;  // for PWM frequency of 1kHz // pin 3
  OCR2A = 0xF8;

  pinMode(CP_OUT, OUTPUT);
  pinMode(READY_Charge_LED, OUTPUT);
  pinMode(Fault_LED, OUTPUT);
  pinMode(Power_LED, OUTPUT);
  pinMode(POWER_RELAY, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(CP_OUT, OUTPUT);
  pinMode(CP_IN, INPUT);

  analogWrite(CP_OUT, 255);
  digitalWrite(POWER_RELAY, LOW);
  digitalWrite(Power_LED, 1);

  // Initialize LCD
  lcd.init();
  Wire.begin();           // Initialize I2C communication
  Wire.setClock(400000);  // Set I2C speed to 400kHz (Fast Mode)
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EVSS Level 1");
  lcd.setCursor(0, 1);
  lcd.print("Power ON");

  lcd.setCursor(0, 3);
  lcd.print("Checking Earth...");
  unsigned long Oscilate = pulseIn(EARTH_PIN, HIGH, 50000);
  Checking_Earth();  //checking earth
  delay(1000);

  lcd.setCursor(0, 3);
  lcd.print("Checking GFCI...");
  digitalRead(Fault_PIN);
  Checking_GFCI();
  delay(1000);
}

void Checking_GFCI() {
  if (digitalRead(Fault_PIN) == 1) {
    digitalWrite(Power_LED, LOW);
    analogWrite(CP_OUT, 255);
    digitalWrite(READY_Charge_LED, LOW);
    digitalWrite(POWER_RELAY, LOW);
    digitalWrite(Fault_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(50);
    digitalWrite(BUZZER, LOW);
    digitalWrite(Fault_LED, LOW);
    delay(50);
    buzzerActivated = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Warning!");
    lcd.setCursor(0, 2);
    lcd.print("GFCI = fail");

  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EVSS Level 1");
    lcd.setCursor(0, 1);
    lcd.print("Power ON");
    lcd.setCursor(0, 3);
    lcd.print("GFCI = OK");
    // delay(1000);
  }
}
void Checking_Earth() {
  if (Oscilate > 100) {  // Earth pin oscillation detected
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EVSS Level 1");
    lcd.setCursor(0, 1);
    lcd.print("Power ON");
    lcd.setCursor(0, 3);
    lcd.print("Earth verified");
    delay(1000);
  } else {
    // No earth detected
    analogWrite(CP_OUT, 255);
    digitalWrite(Power_LED, LOW);
    digitalWrite(READY_Charge_LED, LOW);
    digitalWrite(Fault_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(300);
    digitalWrite(Fault_LED, LOW);
    digitalWrite(BUZZER, LOW);
    delay(300);
    buzzerActivated = false;  // Reset buzzer flag
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("WARNING");
    lcd.setCursor(5, 2);
    lcd.print("No Earth");
  }
}
void findPeakVoltage() {
  peak_voltage = 0;  // Reset the peak voltage

  for (int i = 0; i < 1000; i++) {
    int current_voltage = analogRead(CP_IN);  // Read the current voltage
    if (current_voltage > peak_voltage) {
      peak_voltage = current_voltage;  // Update the peak voltage if current reading is higher
    }
  }
}

void displayTime() {
  if (timerRunning) {
    elapsedTime = millis() - startTime;
  }
  int displaySeconds = (elapsedTime / 1000) % 60;
  int displayMinutes = (elapsedTime / 60000) % 60;
  int displayHours = (elapsedTime / 3600000);
  lcd.setCursor(0, 1);
  lcd.print("Time = ");

  lcd.setCursor(7, 1);
  lcd.print((displayHours / 10) % 10);
  lcd.print(displayHours % 10);
  lcd.print("h ");
  lcd.print((displayMinutes / 10) % 10);
  lcd.print(displayMinutes % 10);
  lcd.print("min ");
  lcd.print((displaySeconds / 10) % 10);
  lcd.print(displaySeconds % 10);
  lcd.print("s");
  // delay(100);  // Small delay to debounce buttons
}

void OV_UV_OC_condition() {
  analogWrite(CP_OUT, 255);
  digitalWrite(POWER_RELAY, LOW);
  digitalWrite(READY_Charge_LED, LOW);
  digitalWrite(Power_LED, LOW);
  digitalWrite(Fault_LED, HIGH);
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(Fault_LED, LOW);
  digitalWrite(BUZZER, LOW);
  delay(100);
  buzzerActivated = false;  // Reset buzzer flag
}

void GFCI_selftest() {
  analogWrite(CP_OUT, 255);
  digitalWrite(Power_LED, LOW);
  digitalWrite(READY_Charge_LED, LOW);
  digitalWrite(POWER_RELAY, LOW);
  digitalWrite(Fault_LED, HIGH);
  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  digitalWrite(Fault_LED, LOW);
  delay(100);
  buzzerActivated = false;
}

void Disp_Power() {
  // Read the data from the sensor
  float voltage = pzem.voltage();
  float current = pzem.current();
  float power = pzem.power();
  float energy = pzem.energy();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("EVSS L1 :");
  lcd.print(amp);
  lcd.print("A");
  displayTime();

  lcd.setCursor(0, 2);
  lcd.print("V= ");
  lcd.print(voltage);
  lcd.print("V");

  lcd.setCursor(11, 2);
  lcd.print("P=");
  lcd.print(power, 0);
  lcd.print(" W");

  lcd.setCursor(0, 3);
  lcd.print("I= ");
  lcd.print(current);
  lcd.print("A");

  lcd.setCursor(11, 3);
  lcd.print("E=");
  lcd.print(energy);
  lcd.print("kWh");

  if (voltage <= 200) {  // Undervoltage condition
    OV_UV_OC_condition();
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("WARNING");
    lcd.setCursor(4, 1);
    lcd.print("UnderVoltage");
    lcd.setCursor(0, 3);
    lcd.print("restart after 1 min");
    undervoltageCondition = true;
    delay(wait);
  } else if (undervoltageCondition && voltage > 200) {  // Recovery from undervoltage
    undervoltageCondition = false;
    handleStateA();
    handleStateB();
    handleStateC();
  }

  if (voltage > 240) {  // over_voltage condition
    OV_UV_OC_condition();
    analogWrite(CP_OUT, 255);
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("WARNING");
    lcd.setCursor(4, 1);
    lcd.print("Overvoltage");
    lcd.setCursor(0, 3);
    lcd.print("restart after 1 min");
    overvoltageCondition = true;
    delay(wait);
  } else if (overvoltageCondition && voltage < 240) {  // Recovery from over_voltage
    overvoltageCondition = false;
    handleStateA();
    handleStateB();
    handleStateC();
  }
  if (current > 20) {  // over_current condition
    OV_UV_OC_condition();
    analogWrite(CP_OUT, 255);
    lcd.clear();
    lcd.setCursor(6, 0);
    lcd.print("WARNING");
    lcd.setCursor(4, 1);
    lcd.print("UnderVoltage");
    lcd.setCursor(0, 3);
    lcd.print("restart after 1 min");
    overCurrentCondition = true;
    delay(wait);
  } else if (overCurrentCondition && current < 20) {  // Recovery from over_voltage
    delay(wait);
    overCurrentCondition = false;
    handleStateA();
    handleStateB();
    handleStateC();
  }
}

void handleStateA() {
  if (digitalRead(Fault_PIN) == 1) {
    GFCI_selftest();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EVSS L1");

    lcd.setCursor(0, 2);
    lcd.print("GFCI Selftest = fail");
    // displayTime();
    // elapsedTime = 0;
    // timerRunning = false;
    // displayTime();
  } else {
    pzem.resetEnergy();
    amp = EEPROM.read(0);

    if (digitalRead(set_Current_PIN) == 0) {
      digitalWrite(BUZZER, 1);
      delay(50);
      buzzerActivated = false;
      if (amp < 15) {
        amp += 3;  // Increase by 3
      } else if (amp < 16) {
        amp += 1;  // Increase by 1 when amp reaches 15
      } else {
        amp = 6;  // Reset to 6 when it goes beyond 16
      }
      EEPROM.write(0, amp);  // Store updated amp value in EEPROM
    }

    analogWrite(CP_OUT, 255);
    digitalWrite(Power_LED, HIGH);
    digitalWrite(Fault_LED, LOW);
    digitalWrite(READY_Charge_LED, HIGH);
    digitalWrite(BUZZER, LOW);
    digitalWrite(POWER_RELAY, LOW);
    buzzerActivated = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("EVSS L1 :");
    lcd.print(amp);
    lcd.print("A");
    lcd.setCursor(14, 0);
    lcd.print("Ready");
    lcd.setCursor(0, 2);
    lcd.print("GFCI = OK");
    displayTime();
    elapsedTime = 0;
    timerRunning = false;
    displayTime();

    if (Oscilate > 100) {  // Earth pin oscillation detected
      lcd.setCursor(11, 2);
      lcd.print("Earth= OK");
    } else {
      // No earth detected
      lcd.setCursor(11, 2);
      lcd.print("Earth =No");
    }
  }
}

void handleStateB() {
  digitalWrite(POWER_RELAY, LOW);
  analogWrite(CP_OUT, 22 + ((255 * (amp / 0.6)) / 100));
  // analogWrite(CP_OUT, 255*0.26);
  digitalWrite(Power_LED, HIGH);
  digitalWrite(Fault_LED, LOW);
  digitalWrite(READY_Charge_LED, HIGH);
  digitalWrite(BUZZER, LOW);
  buzzerActivated = false;
  if (timerRunning) {
    elapsedTime = millis() - startTime;
    timerRunning = false;
  }
  lcd.clear();
  displayTime();
  Disp_Power();
  lcd.setCursor(14, 0);
  lcd.print("Plugin");
}

void handleStateC() {
  digitalWrite(POWER_RELAY, HIGH);
  analogWrite(CP_OUT, 22 + ((255 * (amp / 0.6)) / 100));
  digitalWrite(Power_LED, LOW);
  digitalWrite(Fault_LED, LOW);
  digitalWrite(READY_Charge_LED, HIGH);
  if (!buzzerActivated) {
    digitalWrite(BUZZER, HIGH);
    delay(500);  // Short delay for buzzer
    digitalWrite(BUZZER, LOW);
    buzzerActivated = true;  // Set buzzer flag to prevent reactivation
  }
  while (peak_voltage >= stateC_Thres_min && peak_voltage < stateC_Thres_max) {
    findPeakVoltage();  // Update the peak voltage

    if (!timerRunning) {
      startTime = millis() - elapsedTime;
      timerRunning = true;
    }

    lcd.setCursor(14, 0);
    lcd.print("CHARGE");
    Disp_Power();

    if (digitalRead(Fault_PIN) == 1) {
      while (1) {
        digitalWrite(Power_LED, LOW);
        analogWrite(CP_OUT, 255);
        digitalWrite(READY_Charge_LED, LOW);
        digitalWrite(POWER_RELAY, LOW);
        digitalWrite(Fault_LED, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(50);
        digitalWrite(BUZZER, LOW);
        digitalWrite(Fault_LED, LOW);
        delay(50);
        buzzerActivated = false;
        lcd.clear();
        lcd.setCursor(0, 2);
        lcd.print("GFCI = fail");
        // lcd.setCursor(0, 3);
        // lcd.print("reset to restart");
      }
    }
    // Checking_GFCI();
    // delay(1000);
    if (peak_voltage < stateC_Thres_min || peak_voltage >= stateC_Thres_max) {
      break;
    }
  }
}
void loop() {

  // if (Oscilate > 100) {  // Earth pin oscillation detected || if this section on (if no earth code will not work until earth detected code will work)
  //   {
  //     lcd.setCursor(0, 0);
  //     lcd.print("EVSS L1");
  //     lcd.setCursor(0, 1);
  //     lcd.print("Earth Detected");
  //     delay(1000);
  //     Serial.print("Custom Address:");
  //     Serial.println(pzem.readAddress(), HEX);
  //     // lcd.setCursor(8, 0);
  //     // lcd.print(peak_voltage);
  //     // Serial.println(peak_voltage);
  //   }

  findPeakVoltage();  // Call the function to find the peak voltage
  // lcd.clear();
  // lcd.setCursor(0, 3);
  // lcd.print(peak_voltage);
  // Serial.println(peak_voltage);

  if (peak_voltage >= stateA_Thres_min && peak_voltage <= stateA_Thres_max) {
    handleStateA();
  } else if (peak_voltage >= stateB_Thres_min && peak_voltage < stateB_Thres_max) {
    handleStateB();
  } else if (peak_voltage >= stateC_Thres_min && peak_voltage < stateB_Thres_max) {
    handleStateC();
  } else {  // Error state
    analogWrite(CP_OUT, 255);
    digitalWrite(POWER_RELAY, LOW);
    digitalWrite(Fault_LED, LOW);
    digitalWrite(BUZZER, LOW);
    delay(300);  // Short delay for LED blink
    digitalWrite(Fault_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(300);  // Short delay for LED blink
    digitalWrite(Power_LED, LOW);
    digitalWrite(READY_Charge_LED, LOW);
    buzzerActivated = false;  // Reset buzzer flag
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Warning Error");
  }
  //  }   else {
  //   // No earth detected
  //   analogWrite(CP_OUT, 255);
  //   digitalWrite(Power_LED, LOW);
  //   digitalWrite(READY_Charge_LED, LOW);
  //   digitalWrite(Fault_LED, HIGH);
  //   digitalWrite(BUZZER, HIGH);
  //   delay(300);
  //   digitalWrite(Fault_LED, LOW);
  //   digitalWrite(BUZZER, LOW);
  //   delay(300);
  //   buzzerActivated = false;  // Reset buzzer flag
  //   lcd.clear();
  //   lcd.setCursor(6, 0);
  //   lcd.print("WARNING");
  //   lcd.setCursor(5, 1);
  //   lcd.print("No Earth");
  // }
}
