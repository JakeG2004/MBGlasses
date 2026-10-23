// ESP32-H2 broadcaster / receiver.
//
//   - GPIO (kInputPin) pulled to GND: yellow flash that fades out, broadcast
//     as 802.15.4 frames (kNumChannels x RGB). After the flash, kDeadPackets
//     all-black frames are sent, then the radio goes back to listening.
//   - If isSerpentine: then keep broadcasting a flowing rainbow (also shown on
//     the strips). Otherwise: listen, and mirror the last received frame.
//
// Board: ESP32H2 Dev Module, USB CDC On Boot: Disabled, Zigbee Mode: Disabled
// Library: Adafruit NeoPixel

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "esp_ieee802154.h"
#include "esp_ieee802154_types.h"

extern "C" {
esp_err_t esp_ieee802154_set_auto_ack_rx(bool enable) __attribute__((weak));
esp_err_t esp_ieee802154_set_auto_ack_tx(bool enable) __attribute__((weak));
}

// ---- Pins / timing ----
static const uint8_t kInputPin = D9;
static const uint8_t kStripPinA = D6;
static const uint8_t kStripPinB = D7;
static const uint32_t kFlashFadeMs = 200;  // yellow flash fade duration
static const uint32_t kFrameMs = 10;       // broadcast rate (100 Hz)
static const uint16_t kDeadPackets = 100;  // all-black packets after the flash
static const uint32_t kRainbowCycleMs = 1500;  // time for one full rainbow cycle
static const uint8_t kRainbowRepeats = 1;      // full rainbows across the strip

// true : after the flash, keep broadcasting a flowing per-pixel rainbow
// false: after the flash, go quiet and listen
static const bool isSerpentine = false;

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
static uint32_t flashStartMs = 0;
static uint32_t lastFrameMs = 0;
static uint16_t deadRemaining = 0;
static bool flashActive = false;

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
static void fillStrips(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = Adafruit_NeoPixel::Color(r, g, b);
  stripA.fill(c);
  stripB.fill(c);
  stripA.show();
  stripB.show();
}

static void showStrips(const uint8_t *r, const uint8_t *g, const uint8_t *b) {
  for (int i = 0; i < kStripLen; i++) {
    uint32_t c = Adafruit_NeoPixel::Color(r[i], g[i], b[i]);
    stripA.setPixelColor(i, c);
    stripB.setPixelColor(i, c);
  }
  stripA.show();
  stripB.show();
}

// ---- Colors ----
// Hue 0-1535 (6 regions x 256 steps), full saturation and value.
static void hueToRgb(uint16_t hue, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t region = hue >> 8;
  uint8_t rise = hue & 0xFF;
  uint8_t fall = 255 - rise;
  switch (region) {
    case 0: r = 255;  g = rise; b = 0;    break;
    case 1: r = fall; g = 255;  b = 0;    break;
    case 2: r = 0;    g = 255;  b = rise; break;
    case 3: r = 0;    g = fall; b = 255;  break;
    case 4: r = rise; g = 0;    b = 255;  break;
    default: r = 255; g = 0;    b = fall; break;
  }
}

static void fillArrays(uint8_t *r, uint8_t *g, uint8_t *b,
                       uint8_t cr, uint8_t cg, uint8_t cb) {
  for (int i = 0; i < kNumChannels; i++) {
    r[i] = cr;
    g[i] = cg;
    b[i] = cb;
  }
}

static void rainbowArrays(uint32_t now, uint8_t *r, uint8_t *g, uint8_t *b) {
  uint16_t hue = (uint16_t)(((uint64_t)(now % kRainbowCycleMs) * 1536) / kRainbowCycleMs);
  for (int i = 0; i < kNumChannels; i++) {
    uint16_t h = (hue + (uint32_t)i * 1536 * kRainbowRepeats / kNumChannels) % 1536;
    hueToRgb(h, r[i], g[i], b[i]);
  }
}

// ---- Transmit: one color per channel ----
static void sendFrame(const uint8_t *r, const uint8_t *g, const uint8_t *b) {
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
    radioFrame[10 + i * 3 + 0] = r[i];
    radioFrame[10 + i * 3 + 1] = g[i];
    radioFrame[10 + i * 3 + 2] = b[i];
  }
  txIdle = false;
  if (esp_ieee802154_transmit(radioFrame, false) != ESP_OK) txIdle = true;
}

static void broadcast(uint32_t now) {
  if (!txIdle || (now - lastFrameMs) < kFrameMs) return;

  uint8_t r[kNumChannels], g[kNumChannels], b[kNumChannels];
  uint32_t elapsed = now - flashStartMs;

  if (flashActive && elapsed < kFlashFadeMs) {
    // Yellow flash, fading out.
    uint8_t level = (uint8_t)(255UL * (kFlashFadeMs - elapsed) / kFlashFadeMs);
    fillArrays(r, g, b, level, (uint8_t)(level * 200UL / 255UL), 0);
    deadRemaining = kDeadPackets;
  } else if (deadRemaining > 0) {
    // Dead (all-black) packets after the flash.
    flashActive = false;
    fillArrays(r, g, b, 0, 0, 0);
    deadRemaining--;
  } else if (isSerpentine) {
    // Flowing rainbow, broadcast continuously.
    flashActive = false;
    rainbowArrays(now, r, g, b);
  } else {
    isBroadcasting = false;  // done: go back to listening
    return;
  }

  lastFrameMs = now;
  showStrips(r, g, b);
  sendFrame(r, g, b);
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

  showStrips(r, g, b);
}

void setup() {
  pinMode(kInputPin, INPUT_PULLUP);
  stripA.begin();
  stripB.begin();
  fillStrips(0, 0, 0);
  radioReady = startRadio();
  isBroadcasting = isSerpentine;  // serpentine: start with the rainbow
}

void loop() {
  if (!radioReady) {
    radioReady = startRadio();
    delay(200);
    return;
  }

  uint32_t now = millis();

  // Flash starts on the falling edge of the input pin.
  bool grounded = digitalRead(kInputPin) == LOW;
  if (grounded && !wasGrounded) {
    flashStartMs = now;
    lastFrameMs = now - kFrameMs;  // send the first frame immediately
    flashActive = true;
    isBroadcasting = true;
  }
  wasGrounded = grounded;

  if (isBroadcasting) {
    broadcast(now);
  } else {
    receive();
  }
}
