const uint8_t pins[8] = {14, 15, 16, 17, 18, 19, 9, 10};  // DS1..DS8

void setup() {
  Serial.begin(9600);
  for (uint8_t i = 0; i < 8; i++) pinMode(pins[i], INPUT_PULLUP);
}

void loop() {
  uint8_t id = 0;
  Serial.print("DS1-8 raw: ");
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t v = digitalRead(pins[i]);
    Serial.print(v);                 // 1 = open, 0 = closed (to GND)
    if (!v) id |= (1 << i);          // DS1 = bit 0
  }
  Serial.print("  id=");
  Serial.print(id);
  Serial.print(" (0b");
  Serial.print(id, BIN);
  Serial.println(")");
  delay(300);
}
