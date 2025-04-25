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

// ✅ Updated callback signature to match new ESP-NOW API
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  QueueItem item;
  memcpy(&item, incomingData, sizeof(item));

  // Optional: Print sender MAC address
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           info->src_addr[0], info->src_addr[1], info->src_addr[2],
           info->src_addr[3], info->src_addr[4], info->src_addr[5]);
  Serial.print("Received from: ");
  Serial.println(macStr);

  // Add to queue
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

  esp_now_register_recv_cb(OnDataRecv); // ✅ Now matches updated function
  Serial.println("Doctor Node Ready.");
}

void loop() {
  // Optional: Let user remove patients from the queue via Serial input
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
