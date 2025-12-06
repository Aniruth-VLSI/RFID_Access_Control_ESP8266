#include <SPI.h>
#include <MFRC522.h>
#define SS_PIN  D2    
#define RST_PIN D1   
#define LED_PIN 2     

MFRC522 mfrc522(SS_PIN, RST_PIN);

// Authorized tag(s) - your tag already included
const byte AUTH_UID_COUNT = 1;
const byte AUTH_UID_LEN   = 4;
const byte authorizedUIDs[AUTH_UID_COUNT][AUTH_UID_LEN] = {
  { 0x23, 0x1E, 0xBC, 0x13 } 
};

// Timing
const unsigned long SCAN_DEBOUNCE_MS = 300;   // delay between scans
const unsigned long PRESENCE_CHECK_MS = 250; // how often to check if the card is still present
void printUid(const MFRC522::Uid &uid) {
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(uid.uidByte[i], HEX);
    if (i < uid.size - 1) Serial.print(' ');
  }
  Serial.println();
}

bool equalsUid(const MFRC522::Uid &uid, const byte rows, const byte col, const byte table[][4]) {
  if (uid.size != AUTH_UID_LEN) return false;
  for (byte r = 0; r < rows; r++) {
    bool match = true;
    for (byte i = 0; i < col; i++) {
      if (table[r][i] != uid.uidByte[i]) { match = false; break; }
    }
    if (match) return true;
  }
  return false;
}

void ledOn()  { digitalWrite(LED_PIN, LOW); }  // active LOW
void ledOff() { digitalWrite(LED_PIN, HIGH); }

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println(F("RFID Access — Auto-read mode"));
  pinMode(LED_PIN, OUTPUT);
  ledOff(); // default off

  SPI.begin();
  mfrc522.PCD_Init();
  Serial.println(F("MFRC522 initialised. Present a tag to read..."));
}

void loop() {
  // Quick, non-blocking check: is there any new card in the field?
  if (! mfrc522.PICC_IsNewCardPresent()) {
    delay(SCAN_DEBOUNCE_MS);
    return;
  }

  // Try to read it
  if (! mfrc522.PICC_ReadCardSerial()) {
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(SCAN_DEBOUNCE_MS);
    return;
  }
  // We successfully read the UID once
  Serial.print(F("Card UID: "));
  printUid(mfrc522.uid);

  bool authorized = equalsUid(mfrc522.uid, AUTH_UID_COUNT, AUTH_UID_LEN, authorizedUIDs);

  if (authorized) {
    Serial.println(F(">>> ACCESS GRANTED"));
    // Blink the LED continuously while the SAME card is present
    unsigned long lastBlink = 0;
    bool ledState = false; // track for blinking
    // Keep blinking while card remains in field
    while (true) {
      // blink timing (toggle every 300 ms)
      if (millis() - lastBlink >= 300) {
        lastBlink = millis();
        ledState = !ledState;
        if (ledState) ledOn(); else ledOff();
      }

      // Check whether the same card is still present:
      // We check by asking for a card present and trying to read serial.
      // If reading fails for N consecutive tries, assume card removed.
      bool present = false;
      if (mfrc522.PICC_IsNewCardPresent()) {
        if (mfrc522.PICC_ReadCardSerial()) {
          // read again - if UID matches the one we stored, it's still the same card
          if (mfrc522.uid.size == AUTH_UID_LEN) {
            bool same = true;
            for (byte i = 0; i < AUTH_UID_LEN; i++) {
              if (mfrc522.uid.uidByte[i] != authorizedUIDs[0][i]) { same = false; break; }
            }
            if (same) present = true;
            // if a different tag is detected, treat it as still 'present' for continuity,
            // but we won't continue ACCESS GRANTED for a different tag.
          }
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
        }
      } else {
        // Try a short direct read attempt (sometimes PICC_IsNewCardPresent misses)
        if (mfrc522.PICC_ReadCardSerial()) {
          // got a read - check if UID matches
          bool same = true;
          if (mfrc522.uid.size == AUTH_UID_LEN) {
            for (byte i = 0; i < AUTH_UID_LEN; i++) {
              if (mfrc522.uid.uidByte[i] != authorizedUIDs[0][i]) { same = false; break; }
            }
            if (same) present = true;
          }
          mfrc522.PICC_HaltA();
          mfrc522.PCD_StopCrypto1();
        }
      }

      // If not present, break out and stop blinking
      if (!present) break;

      delay(PRESENCE_CHECK_MS); // small pause before next presence/blink check
    }

    // card removed
    ledOff();
    Serial.println(F("Card removed - stopping ACCESS GRANTED state."));
  } else {
    Serial.println(F(">>> ACCESS DENIED"));
    // short visual feedback for denied
    for (int i = 0; i < 2; ++i) {
      ledOn();
      delay(120);
      ledOff();
      delay(80);
    }
  }

  // Halt and cleanup after handling the card
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  delay(300); // small pause to avoid immediate re-trigger
}
