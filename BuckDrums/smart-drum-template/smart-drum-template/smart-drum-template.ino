// ESP32-H2 broadcaster / receiver.
//
//   - kSelectPin (falling edge): cycle the *current effect*.
//   - kInputPin pulled to GND: play the current effect, broadcast as 802.15.4
//     frames (kNumChannels x RGB) and shown on the local strips. When the
//     effect finishes, the radio goes back to listening.
//   - Otherwise: listen, and mirror the last received frame.
//
// Board: ESP32H2 Dev Module, USB CDC On Boot: Disabled, Zigbee Mode: Disabled
// Library: Adafruit NeoPixel

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include <Wire.h>
#include <I2CKeyPad.h>

#include "esp_ieee802154.h"
#include "esp_ieee802154_types.h"

extern "C" {
esp_err_t esp_ieee802154_set_auto_ack_rx(bool enable) __attribute__((weak));
esp_err_t esp_ieee802154_set_auto_ack_tx(bool enable) __attribute__((weak));
}

// ---- Pins / timing ----
static const uint8_t kInputPin = D9;    // trigger: broadcast current effect
static const uint8_t kSelectPin = D8;   // select next effect (change to a free pin on your board)
static const uint8_t kStripPinA = D6;
static const uint8_t kStripPinB = D7;
static const uint32_t kFrameMs = 10;       // broadcast rate (100 Hz)
static const uint32_t kDeadMs = 1000;      // all-black time after a flash
static const uint32_t kDebounceMs = 200;

// ---- Radio settings (match the glasses) ----
static const uint8_t kChannel = 12;
static const uint16_t kPanId = 2015;
static const uint16_t kSrcAddr = 0x4201;
static const uint16_t kDestAddr = 0xFFFF;
static const uint8_t kMacHeaderBytes = 9;
static const uint8_t kFcsBytes = 2;
static const uint8_t kMaxFrameBytes = 128;
static const uint8_t kNumChannels = 34;
static const uint8_t kStripLen = 34;
static const uint8_t kPayloadBytes = kNumChannels * 3;
static const int8_t kTxPowerDbm = 20;

// ---- ISR -> loop() handoff ----
// frameSeq is a sequence lock: odd while the ISR is writing, even when stable.
static volatile uint32_t frameSeq = 0;
static volatile bool colorPending = false;
static volatile uint8_t pendingR[kStripLen];
static volatile uint8_t pendingG[kStripLen];
static volatile uint8_t pendingB[kStripLen];
static volatile uint8_t pendingCount = 0;

// ---- State ----
static Adafruit_NeoPixel stripA(kStripLen, kStripPinA, NEO_GRB + NEO_KHZ800);
static Adafruit_NeoPixel stripB(kStripLen, kStripPinB, NEO_GRB + NEO_KHZ800);

static volatile bool txIdle = true;
static bool radioReady = false;
static bool callbacksRegistered = false;
static bool radioEnabled = false;

static uint8_t radioFrame[kMaxFrameBytes];
static uint8_t seq = 0;

static bool isBroadcasting = false;
static bool wasGrounded = false;
static bool wasSelectGrounded = false;
static uint32_t lastFrameMs = 0;
static uint32_t lastSelectMs = 0;
static bool stopping = false;          // looping effect released, sending black
static uint32_t deadRemainingMs = 0;

// Keypad things
const int SDA_PIN = 2;
const int SCL_PIN = 3;
char keyMap[19] = "123A456B789C*0#DNF";
char lastKey = 'N';

// ---- Effects ----
enum PatternType {
  SHOW,   // hold c1
  FADE,   // linear c1 -> c2
  DARK,   // black
};

struct Color {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct Pattern {
  PatternType type;
  uint32_t duration;  // ms
  Color c1;
  Color c2;
  uint32_t elapsed;   // ms

  Color ColorNow() const {
    switch (type) {
      case SHOW: return c1;
      case DARK: return Color{0, 0, 0};
      case FADE: {
        int32_t d = duration ? (int32_t)duration : 1;
        int32_t e = (int32_t)min(elapsed, duration);
        return Color{
          (uint8_t)(c1.r + ((int32_t)c2.r - c1.r) * e / d),
          (uint8_t)(c1.g + ((int32_t)c2.g - c1.g) * e / d),
          (uint8_t)(c1.b + ((int32_t)c2.b - c1.b) * e / d),
        };
      }
    }
    return Color{0, 0, 0};
  }
};

static const uint8_t kMaxPatterns = 6;

struct Effect {
  const char* name;
  Pattern patterns[kMaxPatterns];
  uint8_t count;
  uint8_t cur;
  bool loop;

  void Reset() {
    cur = 0;
    for (uint8_t i = 0; i < count; i++) patterns[i].elapsed = 0;
  }

  // Advance by delta ms. Returns false once the effect has finished
  // (never, for a looping effect with at least one non-zero pattern).
  bool Step(uint32_t delta, Color& out) {
    for (uint8_t pass = 0; pass < 2; pass++) {  // 2nd pass = after a loop restart
      while (cur < count) {
        Pattern& p = patterns[cur];
        if (p.elapsed >= p.duration) {
          cur++;
          continue;
        }
        out = p.ColorNow();
        p.elapsed += delta;
        return true;
      }
      if (!loop) return false;
      Reset();
    }
    return false;  // all patterns have zero duration
  }
};

static const Color kBlack = {0, 0, 0};
static const Color kYellow = {255, 200, 0};
static const Color kRed = {255, 0, 0};
static const Color kWhite = {255, 255, 255};

static Effect effects[] = {
  {"Yellow flash", {
    {FADE, 1000, kYellow, kBlack, 0},
    {DARK, kDeadMs, kBlack, kBlack, 0},
  }, 2, 0, false},
  {"Red flash", {
    {FADE, 1000, kRed, kBlack, 0},
    {DARK, kDeadMs, kBlack, kBlack, 0},
  }, 2, 0, false},
  {"White hold", {
    {SHOW, 500, kWhite, kWhite, 0},
    {FADE, 500, kWhite, kBlack, 0},
    {DARK, kDeadMs, kBlack, kBlack, 0},
  }, 3, 0, false},
  {"Yellow-red", {
    {FADE, 700, kYellow, kRed, 0},
    {FADE, 700, kRed, kBlack, 0},
    {DARK, kDeadMs, kBlack, kBlack, 0},
  }, 3, 0, false},
  {"Yellow-red pulse (loop)", {
    {FADE, 500, kYellow, kRed, 0},
    {FADE, 500, kRed, kYellow, 0},
  }, 2, 0, true},
};
static const uint8_t kNumEffects = sizeof(effects) / sizeof(effects[0]);

// The effect that will be broadcast on the next trigger.
static Effect* currentEffect = &effects[0];

static void selectEffect(uint8_t index) {
  if (index >= kNumEffects) index = 0;
  currentEffect = &effects[index];
}

static void selectNextEffect() {
  uint8_t idx = (uint8_t)(currentEffect - effects);
  selectEffect((uint8_t)((idx + 1) % kNumEffects));
}

// ---- Radio callbacks ----
extern "C" void IRAM_ATTR onTxDone(const uint8_t *, const uint8_t *,
                                   esp_ieee802154_frame_info_t *) {
  txIdle = true;
}

extern "C" void IRAM_ATTR onTxFailed(const uint8_t *, esp_ieee802154_tx_error_t) {
  txIdle = true;
}

// Copy this frame's pixels (up to kStripLen) into the pending arrays.
static bool IRAM_ATTR storeFrame(const uint8_t *frame) {
  uint8_t psduLen = frame[0] & 0x7f;
  if (psduLen < kMacHeaderBytes + kFcsBytes) return false;

  uint16_t n = (uint16_t)(psduLen - kMacHeaderBytes - kFcsBytes) / 3;
  if (n > kStripLen) n = kStripLen;

  uint16_t idx = 1u + kMacHeaderBytes;
  if (idx + n * 3 > kMaxFrameBytes) return false;

  frameSeq++;  // odd: write in progress
  for (uint16_t i = 0; i < n; i++) {
    pendingR[i] = frame[idx++];
    pendingG[i] = frame[idx++];
    pendingB[i] = frame[idx++];
  }
  pendingCount = (uint8_t)n;
  frameSeq++;  // even: stable
  return true;
}

// ISR context: copy only; strips stay in loop().
extern "C" void IRAM_ATTR esp_ieee802154_receive_done(uint8_t *frame,
                                                      esp_ieee802154_frame_info_t *) {
  if (frame != nullptr && storeFrame(frame)) {
    colorPending = true;
  }
  esp_ieee802154_receive_handle_done(frame);
}

// A failed receive must not clear the strips.
extern "C" void esp_ieee802154_receive_failed(uint16_t) {}

static bool startRadio() {
  if (!callbacksRegistered) {
    esp_ieee802154_event_cb_list_t callbacks = {};
    callbacks.tx_done_cb = onTxDone;
    callbacks.tx_failed_cb = onTxFailed;
    callbacks.rx_done_cb = esp_ieee802154_receive_done;
    if (esp_ieee802154_event_callback_list_register(callbacks) != ESP_OK) return false;
    callbacksRegistered = true;
  }
  if (!radioEnabled) {
    if (esp_ieee802154_enable() != ESP_OK) return false;
    radioEnabled = true;
    esp_ieee802154_set_coordinator(false);
  }
  if (esp_ieee802154_set_auto_ack_tx) esp_ieee802154_set_auto_ack_tx(false);
  if (esp_ieee802154_set_auto_ack_rx) esp_ieee802154_set_auto_ack_rx(false);
  if (esp_ieee802154_set_channel(kChannel) != ESP_OK) return false;
  if (esp_ieee802154_set_panid(kPanId) != ESP_OK) return false;
  if (esp_ieee802154_set_short_address(kSrcAddr) != ESP_OK) return false;
  if (esp_ieee802154_set_promiscuous(true) != ESP_OK) return false;
  if (esp_ieee802154_set_rx_when_idle(true) != ESP_OK) return false;
  esp_ieee802154_set_txpower(kTxPowerDbm);
  return esp_ieee802154_receive() == ESP_OK;
}

static void rearmReceive() {
  esp_ieee802154_state_t state = esp_ieee802154_get_state();
  if (state != ESP_IEEE802154_RADIO_RECEIVE && state != ESP_IEEE802154_RADIO_TRANSMIT) {
    esp_ieee802154_receive();
  }
}

// ---- Strips ----
static void fillStrips(Color col) {
  uint32_t c = Adafruit_NeoPixel::Color(col.r, col.g, col.b);
  stripA.fill(c);
  stripB.fill(c);
  stripA.show();
  stripB.show();
}

// ---- Transmit: one color per channel ----
static void sendFrame(Color col) {
  radioFrame[0] = kMacHeaderBytes + kPayloadBytes + kFcsBytes;
  radioFrame[1] = 0x41;  // data, intra-PAN, no ACK request
  radioFrame[2] = 0x88;  // 16-bit destination and source
  radioFrame[3] = seq++;
  radioFrame[4] = kPanId & 0xff;
  radioFrame[5] = kPanId >> 8;
  radioFrame[6] = kDestAddr & 0xff;
  radioFrame[7] = kDestAddr >> 8;
  radioFrame[8] = kSrcAddr & 0xff;
  radioFrame[9] = kSrcAddr >> 8;
  for (uint8_t i = 0; i < kNumChannels; i++) {
    radioFrame[10 + i * 3 + 0] = col.r;
    radioFrame[10 + i * 3 + 1] = col.g;
    radioFrame[10 + i * 3 + 2] = col.b;
  }
  txIdle = false;
  if (esp_ieee802154_transmit(radioFrame, false) != ESP_OK) txIdle = true;
}

static void broadcast(uint32_t now) {
  uint32_t delta = now - lastFrameMs;
  if (!txIdle || delta < kFrameMs) return;

  Color broadcastColor = kBlack;
  if (stopping) {
    // Looping effect was released: send black packets, then finish.
    if (deadRemainingMs == 0) {
      fillStrips(kBlack);
      isBroadcasting = false;
      return;
    }
    deadRemainingMs = deadRemainingMs > delta ? deadRemainingMs - delta : 0;
  } else if (!currentEffect->Step(delta, broadcastColor)) {
    fillStrips(kBlack);
    isBroadcasting = false;  // effect finished: go back to listening
    return;
  }

  lastFrameMs = now;
  fillStrips(broadcastColor);
  sendFrame(broadcastColor);
}

// ---- Receive: mirror the latest frame on the strips ----
static void receive() {
  rearmReceive();
  if (!colorPending) return;
  colorPending = false;

  uint8_t r[kStripLen], g[kStripLen], b[kStripLen];
  uint32_t s1, s2;
  uint8_t count;
  do {  // consistent snapshot (see frameSeq)
    s1 = frameSeq;
    count = pendingCount;
    for (int i = 0; i < kStripLen; i++) {
      r[i] = pendingR[i];
      g[i] = pendingG[i];
      b[i] = pendingB[i];
    }
    s2 = frameSeq;
  } while ((s1 & 1) || s1 != s2);

  // Short frame: repeat the last pixel across the rest of the strip.
  for (int i = count; count > 0 && i < kStripLen; i++) {
    r[i] = r[count - 1];
    g[i] = g[count - 1];
    b[i] = b[count - 1];
  }

  fillStrips(Color{r[0], g[0], b[0]});
}

void setupLights() {
  pinMode(kInputPin, INPUT_PULLUP);
  pinMode(kSelectPin, INPUT_PULLUP);
  stripA.begin();
  stripB.begin();
  fillStrips(kBlack);
  radioReady = startRadio();
}

void setupKeypad() {
  Wire.begin(SDA_PIN, SCL_PIN);
  keyPad.loadKeyMap(keyMap);
}

void setup() {
  setupLights();
  setupKeypad();
}

void lightLoop() {
  if (!radioReady) {
    radioReady = startRadio();
    delay(200);
    return;
  }

  uint32_t now = millis();

  // Select the next effect on the falling edge of the select pin
  // (ignored while broadcasting so the running effect isn't changed).
  bool selectGrounded = digitalRead(kSelectPin) == LOW;
  if (selectGrounded && !wasSelectGrounded && !isBroadcasting &&
      now - lastSelectMs > kDebounceMs) {
    selectNextEffect();
    lastSelectMs = now;
  }
  wasSelectGrounded = selectGrounded;

  // Broadcast the current effect on the falling edge of the input pin.
  bool grounded = digitalRead(kInputPin) == LOW;
  if (grounded && !wasGrounded) {
    currentEffect->Reset();
    stopping = false;
    lastFrameMs = now - kFrameMs;  // send the first frame immediately
    isBroadcasting = true;
  }
  // A looping effect runs until the trigger pin is released.
  if (isBroadcasting && !stopping && currentEffect->loop && !grounded) {
    stopping = true;
    deadRemainingMs = kDeadMs;
  }
  wasGrounded = grounded;

  if (isBroadcasting) {
    broadcast(now);
  } else {
    receive();
  }
}

void keypadLoop() {
  char key = keyPad.getChar();

  if (key != lastKey) {
    lastKey = key;
  }
}

void loop() {
  lightLoop();
  keypadLoop();
}
