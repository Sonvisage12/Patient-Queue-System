#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include <RTClib.h>
#include <esp_now.h>
#include <WiFi.h>
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

// Replace with Doctor Node MAC address
uint8_t doctorMAC[] = {0x24, 0x6F, 0x28, 0xAA, 0xBB, 0xCC};

struct QueueItem {
  char uid[20];
  char timestamp[25];
  int number;
  bool removeFromQueue;
};

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  QueueItem item;
  memcpy(&item, incomingData, sizeof(item));
  String uid = String(item.uid);

  if (item.removeFromQueue) {
    uidToPatientMap.erase(uid);
    uidToTimestampMap.erase(uid);
    Serial.print("\n✅ Removed from Queue: ");
    Serial.println(uid);
  }
  printQueue();
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    while (1);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting time to compile time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(onDataRecv);
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, doctorMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (!esp_now_is_peer_exist(doctorMAC)) {
    esp_now_add_peer(&peerInfo);
  }

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

  Serial.println("RFID Arrival Point Initialized. Waiting for card...");
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
    Serial.println("Already in queue. Wait for your turn.");
    blinkLED(RED_LED_PIN);
  } else {
    int pid = patientCounter++;
    DateTime now = rtc.now();
    String timeStr = formatDateTime(now);

    uidToPatientMap[uid] = pid;
    uidToTimestampMap[uid] = timeStr;

    int count = prefs.getInt("count", 0);
    String key = "UID_" + String(count);
    prefs.putString((key + "_uid").c_str(), uid);
    prefs.putInt((key + "_num").c_str(), pid);
    prefs.putString((key + "_time").c_str(), timeStr);
    prefs.putInt("count", count + 1);
    prefs.putInt("counter", patientCounter);

    QueueItem item;
    strncpy(item.uid, uid.c_str(), sizeof(item.uid));
    strncpy(item.timestamp, timeStr.c_str(), sizeof(item.timestamp));
    item.number = pid;
    item.removeFromQueue = false;
    esp_now_send(doctorMAC, (uint8_t *)&item, sizeof(item));

    Serial.print("Patient Registered. Assigned Number: ");
    Serial.print(pid);
    Serial.print(" | Time: ");
    Serial.println(timeStr);

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
