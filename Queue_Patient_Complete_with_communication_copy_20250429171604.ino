#include <SPI.h>
#include <MFRC522.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <RTClib.h>
#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <algorithm>


#define RST_PIN  5
#define SS_PIN   4
#define GREEN_LED_PIN 15
#define RED_LED_PIN   2

MFRC522 mfrc522(SS_PIN, RST_PIN);
Preferences prefs;
RTC_DS3231 rtc;

struct QueueEntry {
  String uid;
  String timestamp;
  int number;
};

std::vector<QueueEntry> patientQueue;
int patientCounter = 1;

uint8_t doctorMAC[] = {0x78, 0x42, 0x1C, 0x6C, 0xA8, 0x3C};
//30:C6:F7:44:1D:24
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
    patientQueue.erase(
      std::remove_if(patientQueue.begin(), patientQueue.end(), [uid](const QueueEntry& entry) {
        return entry.uid == uid;
      }),
      patientQueue.end()
    );
    Serial.print("\n✅ Removed from Queue: ");
    Serial.println(uid);
  }
  printQueue();
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  WiFi.mode(WIFI_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  // Trying to remove all saved RFID Card {
    //prefs.begin("rfid-patients", false);
    // prefs.clear();  // ⚠️ This will erase all keys in the "rfid-patients" namespace
     //prefs.end();
   //Serial.println("All registered cards cleared.");

     ///}
  Serial.print("WiFi MAC Address: ");
  Serial.println(WiFi.macAddress());

  Serial.print("WiFi MAC Address: ");
  Serial.println(WiFi.macAddress());
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

  
  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ Error initializing ESP-NOW");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, doctorMAC, 6);
  peerInfo.channel = 1; 
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(doctorMAC)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("❌ Failed to add peer");
      return;
    }
  }

  esp_now_register_send_cb(onDataSent);
 Serial.println("✅ Sending test message in 3 seconds...");
  delay(3000);

  const char *msg = "Hello Doctor!";
  esp_now_send(doctorMAC, (uint8_t *)msg, strlen(msg));


  esp_now_register_recv_cb(onDataRecv);

  prefs.begin("rfid-patients", false);
  patientCounter = prefs.getInt("counter", 1);
  int savedCount = prefs.getInt("count", 0);
  for (int i = 0; i < savedCount; i++) {
    String key = "UID_" + String(i);
    String uid = prefs.getString((key + "_uid").c_str(), "");
    int number = prefs.getInt((key + "_num").c_str(), 0);
    String timestamp = prefs.getString((key + "_time").c_str(), "");
    if (uid != "") {
      patientQueue.push_back({ uid, timestamp, number });
    }
  }

  std::sort(patientQueue.begin(), patientQueue.end(), [](const QueueEntry& a, const QueueEntry& b) {
    return a.timestamp < b.timestamp;
  });

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

  bool alreadyInQueue = std::any_of(patientQueue.begin(), patientQueue.end(), [uid](const QueueEntry& entry) {
    return entry.uid == uid;
  });

  if (alreadyInQueue) {
    Serial.println("Already in queue. Wait for your turn.");
    blinkLED(RED_LED_PIN);
  } else {
    int pid = patientCounter++;
    DateTime now = rtc.now();
    String timeStr = formatDateTime(now);

    QueueEntry newEntry = { uid, timeStr, pid };
    patientQueue.push_back(newEntry);

    std::sort(patientQueue.begin(), patientQueue.end(), [](const QueueEntry& a, const QueueEntry& b) {
      return a.timestamp < b.timestamp;
    });

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
  Serial.println("\n📋 Current Patient Queue (Sorted by Time):");
  for (const auto &entry : patientQueue) {
    Serial.print("Patient No: ");
    Serial.print(entry.number);
    Serial.print(" | UID: ");
    Serial.print(entry.uid);
    Serial.print(" | Time: ");
    Serial.println(entry.timestamp);
  }
  Serial.println("-------------------------\n");
}

void blinkLED(int pin) {
  digitalWrite(pin, LOW);
  delay(1000);
  digitalWrite(pin, HIGH);
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("📤 Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivered 🟢" : "Failed 🔴");

 
}
