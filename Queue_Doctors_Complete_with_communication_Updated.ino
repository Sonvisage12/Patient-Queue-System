#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <map>
#include <queue>
#include <SPI.h>
#include <MFRC522.h>

#define GREEN_LED_PIN 15
#define RED_LED_PIN   2
#define RST_PIN       5
#define SS_PIN        4

MFRC522 mfrc522(SS_PIN, RST_PIN);

struct QueueItem {
  char uid[20];
  char timestamp[25];
  int number;
  bool removeFromQueue;
};

std::map<String, QueueItem> queueMap;
std::queue<String> patientOrder;

uint8_t patientMAC[] = {0x08, 0xD1, 0xF9, 0xD7, 0x50, 0x98};

void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  QueueItem item;
  memcpy(&item, incomingData, sizeof(item));
  String uid = String(item.uid);

  if (!item.removeFromQueue) {
    // Avoid duplicate entries
    if (!queueMap.count(uid)) {
      queueMap[uid] = item;
      patientOrder.push(uid);
      Serial.print("\n📥 New Patient Added: ");
      Serial.print(item.number);
      Serial.print(" | UID: ");
      Serial.print(uid);
      Serial.print(" | Time: ");
      Serial.println(item.timestamp);
      displayNextPatient();
    }
  }
}

void setup() {
  Serial.begin(115200);
  SPI.begin();

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  mfrc522.PCD_Init();

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);

  WiFi.mode(WIFI_STA);
  Serial.print("WiFi MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, patientMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(patientMAC)) {
    esp_now_add_peer(&peerInfo);
  }

  Serial.println("👨‍⚕️ Doctor Node Initialized");
  Serial.println("Waiting for patient queue...");
}

void loop() {
  if (queueMap.empty() || patientOrder.empty()) return;

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  String uid = getUIDString(mfrc522.uid.uidByte, mfrc522.uid.size);
  uid.toUpperCase();

  String currentUID = patientOrder.front();
  currentUID.toUpperCase();

  if (uid == currentUID) {
    QueueItem item = queueMap[currentUID];
    Serial.print("✅ Patient No ");
    Serial.print(item.number);
    Serial.println(" attended. Removing from queue.");

    item.removeFromQueue = true;
    esp_now_send(patientMAC, (uint8_t *)&item, sizeof(item));
    queueMap.erase(currentUID);
    patientOrder.pop();

    blinkLED(GREEN_LED_PIN);
    displayNextPatient();
  } else {
    Serial.println("❌ Not the current patient in queue. Access Denied.");
    blinkLED(RED_LED_PIN);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(1500);
}

void displayNextPatient() {
  while (!patientOrder.empty()) {
    String uid = patientOrder.front();
    if (queueMap.count(uid)) {
      QueueItem item = queueMap[uid];
      Serial.print("🔔 Next Patient Number: ");
      Serial.println(item.number);
      return;
    } else {
      patientOrder.pop(); // Clean up stale entries
    }
  }

  Serial.println("📭 Queue is now empty.");
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
