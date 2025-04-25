#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <RTClib.h>

#define RST_PIN 5
#define SS_PIN  4

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize SPI and RFID
  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println("RFID reader initialized.");

  // Initialize I2C and RTC
  Wire.begin(22, 21);  // SDA = 22, SCL = 21
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    while (1);
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));  // Sets to compile time
    // Or manually set: rtc.adjust(DateTime(2025, 4, 23, 12, 0, 0));
  }

  Serial.println("System ready. Scan a card...");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String uid = getUIDString(mfrc522.uid.uidByte, mfrc522.uid.size);
  DateTime now = rtc.now();

  Serial.print("Card UID: ");
  Serial.println(uid);

  Serial.print("Scanned at: ");
  printDateTime(now);
  Serial.println();

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(1500);  // Debounce
}

String getUIDString(byte *buffer, byte bufferSize) {
  String s = "";
  for (byte i = 0; i < bufferSize; i++) {
    if (buffer[i] < 0x10) s += "0";
    s += String(buffer[i], HEX);
  }
  s.toUpperCase();
  return s;
}

void printDateTime(const DateTime &dt) {
  Serial.print(dt.year()); Serial.print('/');
  Serial.print(dt.month()); Serial.print('/');
  Serial.print(dt.day()); Serial.print(" ");
  Serial.print(dt.hour()); Serial.print(':');
  Serial.print(dt.minute()); Serial.print(':');
  Serial.print(dt.second());
}
