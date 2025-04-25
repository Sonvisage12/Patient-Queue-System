#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <RTClib.h>
#include <map>

#define RST_PIN  5
#define SS_PIN   4

#define GREEN_LED_PIN 15
#define RED_LED_PIN   2

MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences prefs;
RTC_DS3231 rtc;

std::map<String, int> uidToPatientMap;
std::map<String, String> uidToTimestampMap;

int patientCounter = 1;

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  Wire.begin(); // SDA = default GPIO21, SCL = default GPIO22
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to compile time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("RFID Arrival Point Initialized. Waiting for card...");

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);

  // Load saved UIDs
  prefs.begin("rfid-patients", false);
  patientCounter = prefs.getInt("counter", 1);
  int savedCount = prefs.getInt("count", 0);
  for (int i = 0; i < savedCount; i++) {
    String key = "UID_" + String(i);
    String uid = prefs.getString((key + "_uid").c_str(), "");
    int number = prefs.getInt((key + "_num").c_str(), 0);
    String timestamp = prefs.getString((key + "_time").c_str(), "");

    if (uid != "") {
      uidToPatientMap[uid] = number;
      uidToTimestampMap[uid] = timestamp;
    }
  }

  printQueue();
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String uid = getUIDString(mfrc522.uid.uidByte, mfrc522.uid.size);
  Serial.print("Card UID detected: ");
  Serial.println(uid);

  if (uidToPatientMap.count(uid) > 0) {
    int assignedNumber = uidToPatientMap[uid];
    String timestamp = uidToTimestampMap[uid];

    Serial.print("This UID is already registered. Assigned Number: ");
    Serial.print(assignedNumber);
    Serial.print(" | Timestamp: ");
    Serial.println(timestamp);

    blinkLED(RED_LED_PIN);
  } else {
    int pid = patientCounter++;
    DateTime now = rtc.now();
    String timeStr = formatDateTime(now);

    uidToPatientMap[uid] = pid;
    uidToTimestampMap[uid] = timeStr;

    Serial.print("Patient Registered. Assigned Number: ");
    Serial.print(pid);
    Serial.print(" | Time: ");
    Serial.println(timeStr);

    int count = prefs.getInt("count", 0);
    String key = "UID_" + String(count);
    prefs.putString((key + "_uid").c_str(), uid);
    prefs.putInt((key + "_num").c_str(), pid);
    prefs.putString((key + "_time").c_str(), timeStr);
    prefs.putInt("count", count + 1);
    prefs.putInt("counter", patientCounter);

    blinkLED(GREEN_LED_PIN);
    printQueue();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(1500);
}

String getUIDString(byte *buffer, byte bufferSize) {
  String uidString = "";
  for (byte i = 0; i < bufferSize; i++) {
    if (buffer[i] < 0x10) uidString += "0";
    uidString += String(buffer[i], HEX);
  }
  uidString.toUpperCase();
  return uidString;
}

String formatDateTime(const DateTime &dt) {
  char buffer[20];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d", dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
  return String(buffer);
}

void printQueue() {
  Serial.println("\n📋 Current Patient Queue:");
  for (const auto &entry : uidToPatientMap) {
    String uid = entry.first;
    int number = entry.second;
    String time = uidToTimestampMap[uid];
    Serial.print("Patient No: ");
    Serial.print(number);
    Serial.print(" | UID: ");
    Serial.print(uid);
    Serial.print(" | Time: ");
    Serial.println(time);
  }
  Serial.println("-------------------------\n");
}

void blinkLED(int pin) {
  digitalWrite(pin, LOW);
  delay(1000);
  digitalWrite(pin, HIGH);
}
