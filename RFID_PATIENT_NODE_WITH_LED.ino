#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <map>

#define RST_PIN  5
#define SS_PIN   4

#define GREEN_LED_PIN 15
#define RED_LED_PIN   2

MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences prefs;

std::map<String, int> uidToPatientMap;
int patientCounter = 1;

void setup() {
  Serial.begin(115200);
  SPI.begin();

    //prefs.begin("rfid-patients", false);
     //prefs.clear();  // ⚠️ This will erase all keys in the "rfid-patients" namespace
     //prefs.end();
   //Serial.println("All registered cards cleared.");





  
  mfrc522.PCD_Init();
  Serial.println("RFID Arrival Point Initialized. Waiting for card...");

  // LED setup
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
    if (uid != "") {
      uidToPatientMap[uid] = number;
    }
  }
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
    Serial.print("This UID is already registered. Assigned Number: ");
    Serial.println(assignedNumber);

    blinkLED(RED_LED_PIN);
  } else {
    int pid = patientCounter++;
    uidToPatientMap[uid] = pid;

    Serial.print("Patient Registered. Assigned Number: ");
    Serial.println(pid);

    int count = prefs.getInt("count", 0);
    String key = "UID_" + String(count);
    prefs.putString((key + "_uid").c_str(), uid);
    prefs.putInt((key + "_num").c_str(), pid);
    prefs.putInt("count", count + 1);
    prefs.putInt("counter", patientCounter);

    blinkLED(GREEN_LED_PIN);
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

void blinkLED(int pin) {
  digitalWrite(pin, LOW);
  delay(1000);
  digitalWrite(pin, HIGH);
}
