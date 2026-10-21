// ESP32-H2 simple broadcaster: rainbow by default, yellow flash on pin 27.
//
// Sends one 802.15.4 broadcast frame (same layout the glasses already
// expect: 96 bytes = 32 channels x RGB) with every channel set to the same
// color. The onboard RGB LED on GPIO8 mirrors whatever is being broadcast.
//
//   - Default: slow rainbow color flow.
//   - GPIO27 pulled to GND: brief yellow flash that fades out.
//   - Rainbow resumes after 5 seconds with no input on GPIO27.
//
// Board: ESP32H2 Dev Module
// USB CDC On Boot: Disabled (GPIO27 is the USB D+ pin on the H2; see notes)
// Zigbee Mode: Disabled

#include <Arduino.h>
#include <string.h>

#include "esp_ieee802154.h"
#include "esp_ieee802154_types.h"

extern "C" {
esp_err_t esp_ieee802154_set_auto_ack_rx(bool enable) __attribute__((weak));
esp_err_t esp_ieee802154_set_auto_ack_tx(bool enable) __attribute__((weak));
}

// ---- Pins / timing ----
static const uint8_t kInputPin = 27;
static const uint8_t kLedPin = 8;
static const uint8_t kLedBrightness = 16;    // onboard LED only, 0-255 (broadcast stays full)
static const uint32_t kIdleReturnMs = 1500;  // no input -> back to rainbow
static const uint32_t kFlashFadeMs = 300;    // yellow flash fade duration
static const uint32_t kFrameMs = 10;         // broadcast + LED update rate (100 Hz)
static const uint32_t kRainbowCycleMs = 1500; // time for one full rainbow cycle
static const uint8_t kRainbowRepeats = 1;  // full rainbows across the strip

// ---- Radio settings (match the glasses) ----
static const uint8_t kChannel = 12;
static const uint16_t kPanId = 2015;
static const uint16_t kSrcAddr = 0x4201;
static const uint16_t kDestAddr = 0xFFFF;
static const uint8_t kMacHeaderBytes = 9;
static const uint8_t kFcsBytes = 2;
static const uint8_t kNumChannels = 34;
static const uint8_t kPayloadBytes = kNumChannels * 3;
static const int8_t kTxPowerDbm = 20;

static volatile bool txIdle = true;
static bool radioReady = false;
static bool callbacksRegistered = false;
static bool radioEnabled = false;

static uint8_t radioFrame[128];
static uint8_t seq = 0;

bool isSerpentine = false;

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
  esp_ieee802154_set_txpower(kTxPowerDbm);
  return true;
}

// Build a broadcast frame with every channel set to (r, g, b).
static void armFrame(uint8_t r, uint8_t g, uint8_t b) {
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
    radioFrame[10 + i * 3 + 0] = r;
    radioFrame[10 + i * 3 + 1] = g;
    radioFrame[10 + i * 3 + 2] = b;
  }
}

// Build a broadcast frame with every channel set to (r, g, b).
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

static bool hasInput = false;
static bool wasGrounded = false;
static uint32_t lastInputMs = 0;
static uint32_t flashStartMs = 0;
static uint32_t lastFrameMs = 0;
static bool forceSend = false;  // send the next frame immediately (flash onset)

void setup() {
  pinMode(kInputPin, INPUT_PULLUP);
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
  uint8_t r, g, b;

  uint8_t redArr[kNumChannels];
  uint8_t greenArr[kNumChannels];
  uint8_t blueArr[kNumChannels];

  if (hasInput && (now - lastInputMs) < kIdleReturnMs) {
    uint32_t elapsed = now - flashStartMs;
    if (elapsed < kFlashFadeMs) {
      uint8_t level = (uint8_t)(255UL * (kFlashFadeMs - elapsed) / kFlashFadeMs);
      r = level; g = (uint8_t)(level * 200UL / 255UL); b = 0;
    } else {
      r = g = b = 0;
    }
    for (int i = 0; i < kNumChannels; i++) {
      redArr[i] = r; greenArr[i] = g; blueArr[i] = b;
    }
  } else {
    hasInput = false;
    uint16_t hue = (uint16_t)(((uint64_t)(now % kRainbowCycleMs) * 1536) / kRainbowCycleMs);

    if (isSerpentine) {
      makeRainbowPacket(hue, redArr, greenArr, blueArr);
      r = redArr[0]; g = greenArr[0]; b = blueArr[0];
    } else {
      hueToRgb(hue, r, g, b);
      for (int i = 0; i < kNumChannels; i++) {
        redArr[i] = r; greenArr[i] = g; blueArr[i] = b;
      }
    }
  }

  // Mirror the broadcast on the onboard LED, at the frame rate (not every
  // loop pass, which floods the LED driver and makes it look steppy).
  static uint32_t lastLedMs = 0;
  if ((now - lastLedMs) >= kFrameMs) {
    lastLedMs = now;

    if(isSerpentine) {
      neopixelWrite(kLedPin,
                    (uint8_t)((uint16_t)redArr[0] * kLedBrightness / 255),
                    (uint8_t)((uint16_t)greenArr[0] * kLedBrightness / 255),
                    (uint8_t)((uint16_t)blueArr[0] * kLedBrightness / 255));
    } else {
      neopixelWrite(kLedPin,
                    (uint8_t)((uint16_t)r * kLedBrightness / 255),
                    (uint8_t)((uint16_t)g * kLedBrightness / 255),
                    (uint8_t)((uint16_t)b * kLedBrightness / 255));
    }
  }

  // One broadcast to all channels.
  if (txIdle && (forceSend || (now - lastFrameMs) >= kFrameMs)) {
    forceSend = false;
    lastFrameMs = now;
    //armFrame(r, g, b);
    armFrameArr(redArr, greenArr, blueArr);
    txIdle = false;
    if (esp_ieee802154_transmit(radioFrame, false) != ESP_OK) {
      txIdle = true;
    }
  }
}
