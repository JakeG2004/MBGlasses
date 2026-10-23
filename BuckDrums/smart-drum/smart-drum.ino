// ESP32-H2 simple broadcaster: rainbow by default, yellow flash on pin 27.
//
// Sends 802.15.4 broadcast frames (same layout the glasses already
// expect: 96 bytes = 32 channels x RGB; here 34 channels). The same colors
// that are broadcast are mirrored on two 34-pixel NeoPixel strips on D6 and D7.
//
//   - Default: slow rainbow color flow.
//   - GPIO27 pulled to GND: brief yellow flash that fades out.
//   - Rainbow resumes after kIdleReturnMs with no input on GPIO27.
//   - isSerpentine == true : each pixel gets its own hue (flowing rainbow),
//                            and packets are broadcast continuously.
//   - isSerpentine == false: the whole strip is one color, and NO packets are
//                            sent during the rainbow.
//
// Packet policy:
//   - Rainbow + isSerpentine: broadcast every frame.
//   - Rainbow + !isSerpentine: silent.
//   - Yellow flash: broadcast every frame while it fades. Once the flash is
//     done, broadcast kDeadPackets all-black ("dead") packets, then go silent
//     until the next flash (or until the rainbow resumes).
//
// Board: ESP32H2 Dev Module
// USB CDC On Boot: Disabled (GPIO27 is the USB D+ pin on the H2; see notes)
// Zigbee Mode: Disabled
// Library: Adafruit NeoPixel (install via Library Manager)

#include <Arduino.h>
#include <string.h>
#include <Adafruit_NeoPixel.h>

#include "esp_ieee802154.h"
#include "esp_ieee802154_types.h"

extern "C" {
esp_err_t esp_ieee802154_set_auto_ack_rx(bool enable) __attribute__((weak));
esp_err_t esp_ieee802154_set_auto_ack_tx(bool enable) __attribute__((weak));
}

// ---- Pins / timing ----
static const uint8_t kInputPin = D9;
static const uint8_t kStripPinA = D6;        // NeoPixel strip A data
static const uint8_t kStripPinB = D7;        // NeoPixel strip B data
static const uint8_t kLedBrightness = 255;    // strips only, 0-255 (broadcast stays full)
static const uint32_t kIdleReturnMs = 1500;  // no input -> back to rainbow
static const uint32_t kFlashFadeMs = 200;    // yellow flash fade duration
static const uint32_t kFrameMs = 10;         // broadcast + LED update rate (100 Hz)
static const uint32_t kRainbowCycleMs = 1500; // time for one full rainbow cycle
static const uint8_t kRainbowRepeats = 1;    // full rainbows across the strip
static const uint16_t kDeadPackets = 100;    // all-black packets sent after the flash

// ---- Radio settings (match the glasses) ----
static const uint8_t kChannel = 12;
static const uint16_t kPanId = 2015;
static const uint16_t kSrcAddr = 0x4201;
static const uin16_t kShortAddr = 0x4202;
static const uint16_t kDestAddr = 0xFFFF;
static const uint8_t kMacHeaderBytes = 9;
static const uint8_t kFcsBytes = 2;
static const uint8_t kMaxFrameBytes = 128;
static const uint8_t kNumChannels = 34;
static const uint8_t kPayloadBytes = kNumChannels * 3;
static const int8_t kTxPowerDbm = 20;

// ISR -> loop() handoff. frameSeq is a sequence lock: odd while the ISR is
// writing, even when stable. loop() retries its copy if it saw a write.
static volatile uint32_t frameSeq = 0;
static volatile bool colorPending = false;
static volatile uint8_t pendingRArr[kStripLen];
static volatile uint8_t pendingGArr[kStripLen];
static volatile uint8_t pendingBArr[kStripLen];
static volatile uint8_t pendingCount = 0;  // pixels actually present in the last frame

static volatile uint32_t rxCount = 0;
static volatile uint32_t rxShortCount = 0;
static volatile uint32_t rxFailCount = 0;
static bool radioOk = false;

static uint8_t firstFrame[kMaxFrameBytes];
static uint8_t firstFrameLen = 0;
static int8_t firstRssi = 0;
static uint8_t firstLqi = 0;
static volatile bool firstFrameSaved = false;
static volatile bool dumpPending = false;

// ---- NeoPixel strips ----
static Adafruit_NeoPixel stripA(kNumChannels, kStripPinA, NEO_GRB + NEO_KHZ800);
static Adafruit_NeoPixel stripB(kNumChannels, kStripPinB, NEO_GRB + NEO_KHZ800);

static volatile bool txIdle = true;
static bool radioReady = false;
static bool callbacksRegistered = false;
static bool radioEnabled = false;

static uint8_t radioFrame[kMaxFrameBytes];
static uint8_t seq = 0;

bool isSerpentine = false;
bool isBroadcasting = false;

// Both strips show the same per-pixel colors.
static void setStrip(const uint8_t *redArr, const uint8_t *greenArr, const uint8_t *blueArr) {
  for (int i = 0; i < kStripLen; i++) {
    uint32_t color = Adafruit_NeoPixel::Color(redArr[i], greenArr[i], blueArr[i]);
    stripA.setPixelColor(i, color);
    stripB.setPixelColor(i, color);
  }
  stripA.show();
  stripB.show();
}

extern "C" void IRAM_ATTR onTxDone(const uint8_t *frame, const uint8_t *ack,
                                   esp_ieee802154_frame_info_t *ackInfo) {
  (void)frame;
  (void)ack;
  (void)ackInfo;
  txIdle = true;
}

extern "C" void IRAM_ATTR onTxFailed(const uint8_t *frame,
                                     esp_ieee802154_tx_error_t error) {
  (void)frame;
  (void)error;
  txIdle = true;
}

static bool startRadio() {
  if (!callbacksRegistered) {
    esp_ieee802154_event_cb_list_t callbacks = {};
    callbacks.tx_done_cb = onTxDone;
    callbacks.tx_failed_cb = onTxFailed;
    //callbacks.rx_done_cb = esp_ieee802154_receive_done; // new
    if (esp_ieee802154_event_callback_list_register(callbacks) != ESP_OK) {
      return false;
    }
    callbacksRegistered = true;
  }
  if (!radioEnabled) {
    if (esp_ieee802154_enable() != ESP_OK) {
      return false;
    }
    radioEnabled = true;
    esp_ieee802154_set_coordinator(false);
  }
  if (esp_ieee802154_set_auto_ack_tx != nullptr) {
    esp_ieee802154_set_auto_ack_tx(false);
  }
  if (esp_ieee802154_set_auto_ack_rx != nullptr) {
    esp_ieee802154_set_auto_ack_rx(false);
  }
  if (esp_ieee802154_set_channel(kChannel) != ESP_OK) return false;
  if (esp_ieee802154_set_panid(kPanId) != ESP_OK) return false;
  if (esp_ieee802154_set_short_address(kSrcAddr) != ESP_OK) return false;
  //if (esp_ieee802154_set_promiscuous(true) != ESP_OK) return false;  //new
  //if (esp_ieee802154_set_rx_when_idle(true) != ESP_OK) return false; // new
  //esp_ieee802154_set_txpower(kTxPowerDbm);
  return true;
}

// Build a broadcast frame with a separate color per channel.
static void armFrameArr(uint8_t* redArr, uint8_t* greenArr, uint8_t* blueArr) {
  radioFrame[0] = kMacHeaderBytes + kPayloadBytes + kFcsBytes;
  radioFrame[1] = 0x41;  // data, intra-PAN, no ACK request
  radioFrame[2] = 0x88;  // 16-bit destination and source
  radioFrame[3] = seq++;
  radioFrame[4] = (uint8_t)(kPanId & 0xff);
  radioFrame[5] = (uint8_t)(kPanId >> 8);
  radioFrame[6] = (uint8_t)(kDestAddr & 0xff);
  radioFrame[7] = (uint8_t)(kDestAddr >> 8);
  radioFrame[8] = (uint8_t)(kSrcAddr & 0xff);
  radioFrame[9] = (uint8_t)(kSrcAddr >> 8);
  for (uint8_t i = 0; i < kNumChannels; i++) {
    radioFrame[10 + i * 3 + 0] = redArr[i];
    radioFrame[10 + i * 3 + 1] = greenArr[i];
    radioFrame[10 + i * 3 + 2] = blueArr[i];
  }
}

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

static void makeRainbowPacket(uint16_t hue, uint8_t* redArr, uint8_t* greenArr, uint8_t* blueArr) {
  for (int i = 0; i < kNumChannels; i++) {
    uint16_t h = (hue + (uint32_t)i * 1536 * kRainbowRepeats / kNumChannels) % 1536;
    hueToRgb(h, redArr[i], greenArr[i], blueArr[i]);
  }
}

// Push the per-channel colors to both strips. In serpentine mode each pixel
// shows its own channel; otherwise every pixel shows the single color (r,g,b).
static void showStrips(const uint8_t* redArr, const uint8_t* greenArr,
                       const uint8_t* blueArr, uint8_t r, uint8_t g, uint8_t b) {
  for (uint8_t i = 0; i < kNumChannels; i++) {
    uint32_t c;
    if (isSerpentine) {
      c = Adafruit_NeoPixel::Color(redArr[i], greenArr[i], blueArr[i]);
    } else {
      c = Adafruit_NeoPixel::Color(r, g, b);
    }
    stripA.setPixelColor(i, c);
    stripB.setPixelColor(i, c);
  }
  stripA.show();
  stripB.show();
}

static bool hasInput = false;
static bool wasGrounded = false;
static uint32_t lastInputMs = 0;
static uint32_t flashStartMs = 0;
static uint32_t lastFrameMs = 0;
static bool forceSend = false;  // send the next frame immediately (flash onset)
static uint16_t deadRemaining = 0;  // dead packets still to send after a flash

void setup() {
  pinMode(kInputPin, INPUT_PULLUP);

  stripA.begin();
  stripB.begin();
  stripA.setBrightness(kLedBrightness);
  stripB.setBrightness(kLedBrightness);
  stripA.clear();
  stripB.clear();
  stripA.show();
  stripB.show();

  radioReady = startRadio();
}

void loop() {
  if (!radioReady) {
    radioReady = startRadio();
    delay(200);
    return;
  }

  uint32_t now = millis();

  // Input: pin 27 grounded. Flash starts on the falling edge.
  bool grounded = digitalRead(kInputPin) == LOW;
  if (grounded) {
    lastInputMs = now;
    if (!wasGrounded) {
      flashStartMs = now;
      hasInput = true;
      forceSend = true;
    }
  }
  wasGrounded = grounded;

  // Pick the current color.
  uint8_t r = 0, g = 0, b = 0;

  uint8_t redArr[kNumChannels];
  uint8_t greenArr[kNumChannels];
  uint8_t blueArr[kNumChannels];

  bool shouldSend = false;  // does this loop iteration's frame get broadcast?

  if (hasInput && (now - lastInputMs) < kIdleReturnMs) {
    uint32_t elapsed = now - flashStartMs;
    if (elapsed < kFlashFadeMs) {
      // Flash still fading: broadcast it, and keep the dead-packet budget
      // topped up so it's full the moment the flash ends.
      uint8_t level = (uint8_t)(255UL * (kFlashFadeMs - elapsed) / kFlashFadeMs);
      r = level; g = (uint8_t)(level * 200UL / 255UL); b = 0;
      shouldSend = true;
      deadRemaining = kDeadPackets;
    } else {
      // Flash done: send the dead (all-black) packets, then go quiet.
      r = g = b = 0;
      shouldSend = (deadRemaining > 0);
    }
    for (int i = 0; i < kNumChannels; i++) {
      redArr[i] = r; greenArr[i] = g; blueArr[i] = b;
    }
  } else {
    hasInput = false;
    deadRemaining = 0;
    uint16_t hue = (uint16_t)(((uint64_t)(now % kRainbowCycleMs) * 1536) / kRainbowCycleMs);

    if (isSerpentine) {
      makeRainbowPacket(hue, redArr, greenArr, blueArr);
      r = redArr[0]; g = greenArr[0]; b = blueArr[0];
      shouldSend = true;  // only the serpentine rainbow spams packets
    } else {
      uint8_t dark = 0;
      //hueToRgb(hue, r, g, b);
      hueToRgb(hue, dark, dark, dark);
      for (int i = 0; i < kNumChannels; i++) {
        redArr[i] = r; greenArr[i] = g; blueArr[i] = b;
      }
      shouldSend = false;  // non-serpentine rainbow: no packets
    }
  }

  // Mirror the colors on the NeoPixel strips at the frame rate.
  static uint32_t lastLedMs = 0;
  if ((now - lastLedMs) >= kFrameMs) {
    lastLedMs = now;
    showStrips(redArr, greenArr, blueArr, r, g, b);
  }

  // Broadcast to all channels (only when the current state calls for it).
  if (shouldSend && txIdle && (forceSend || (now - lastFrameMs) >= kFrameMs)) {
    forceSend = false;
    lastFrameMs = now;
    armFrameArr(redArr, greenArr, blueArr);
    txIdle = false;
    if (esp_ieee802154_transmit(radioFrame, false) != ESP_OK) {
      txIdle = true;
    } else if (deadRemaining > 0 && r == 0 && g == 0 && b == 0) {
      deadRemaining--;  // count only dead packets that actually went out
    }
  }
}
