#include <Wire.h>
#include <I2CKeyPad.h>

// I2C pins for ESP32-H2
const int SDA_PIN = 2;
const int SCL_PIN = 3;

// Set this to the address your I2C scanner reported
const uint8_t KEYPAD_ADDRESS = 0x20;

I2CKeyPad keyPad(KEYPAD_ADDRESS);

// 4x4 layout. Index 16 = 'N' (no key), index 17 = 'F' (fail / multiple keys)
char keyMap[19] = "123A456B789C*0#DNF";

char lastKey = 'N';

void setup() {
  Serial.begin(115200);
  while (!Serial);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);

  if (!keyPad.begin()) {
    Serial.println("ERROR: Could not find I2C Keypad. Check wiring and address!");
    while (true) delay(1000);  // halt so the error stays visible
  }

  keyPad.loadKeyMap(keyMap);
  Serial.println("Keypad ready. Press a button.");
}

void loop() {
  char key = keyPad.getChar();

  // Print once per press (not repeatedly while the key is held)
  if (key != lastKey) {
    if (key != 'N' && key != 'F') {
      Serial.print("Button pressed: ");
      Serial.println(key);
    }
    lastKey = key;
  }

  delay(20);  // polling interval, also acts as simple debounce
}
