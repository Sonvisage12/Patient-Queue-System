// PatientNode.ino
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "RTClib.h"
#include <esp_now.h>
#include <WiFi.h>
#include <map>

#define RST_PIN 5
#define SS_PIN 4
#define GREEN_LED_PIN 15
#define RED_LED_PIN 2

MFRC522 mfrc522(SS_PIN, RST_PIN);
RTC_DS3231 rtc;

typedef struct {
  char uid[20];
  char timestamp[30];
} QueueItem;

std::map<String, String> queueMap;

uint8_t doctorAddress[] = {0x24, 0x6F, 0x28, 0xAB, 0xCD, 0xEF}; // Replace with actual MAC

void sendQueueToDoctor() {
  for (auto &entry : queueMap) {
    QueueItem item;
    strncpy(item.uid, entry.first.c_str(), sizeof(item.uid));
    strncpy(item.timestamp, entry.second.c_str(), sizeof(item.timestamp));
    esp_now_send(doctorAddress, (uint8_t *)&item, sizeof(item));
  }
}

String getCurrentTimestamp() {
  DateTime now = rtc.now();
  char buf[30];
  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  return String(buf);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, HIGH);

  Wire.begin();
  rtc.begin();

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, doctorAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
  }

  Serial.println("Patient Node Ready.");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  if (queueMap.count(uid) == 0) {
    String timestamp = getCurrentTimestamp();
    queueMap[uid] = timestamp;

    blinkLED(GREEN_LED_PIN);
    Serial.printf("New UID: %s at %s\n", uid.c_str(), timestamp.c_str());
    sendQueueToDoctor();
  } else {
    Serial.println("UID already in queue.");
    blinkLED(RED_LED_PIN);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(1000);
}

void blinkLED(int pin) {
  digitalWrite(pin, LOW);
  delay(500);
  digitalWrite(pin, HIGH);
}
