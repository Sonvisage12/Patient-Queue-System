// DoctorNode.ino
#include <esp_now.h>
#include <WiFi.h>
#include <map>

typedef struct {
  char uid[20];
  char timestamp[30];
} QueueItem;

std::map<String, String> doctorQueue;

void printQueue() {
  Serial.println("Current Queue:");
  for (auto &entry : doctorQueue) {
    Serial.printf("UID: %s | Time: %s\n", entry.first.c_str(), entry.second.c_str());
  }
  Serial.println("-------------");
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  QueueItem item;
  memcpy(&item, incomingData, sizeof(item));
  doctorQueue[String(item.uid)] = String(item.timestamp);
  printQueue();
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  Serial.println("Doctor Node Ready.");
}

void loop() {
  // Here you can simulate RFID scan at doctor node to remove from queue
  // Example: Type UID in Serial Monitor to remove
  if (Serial.available()) {
    String uid = Serial.readStringUntil('\n');
    uid.trim();
    if (doctorQueue.count(uid) > 0) {
      doctorQueue.erase(uid);
      Serial.println("Removed: " + uid);
      printQueue();
    } else {
      Serial.println("UID not in queue.");
    }
  }
}
