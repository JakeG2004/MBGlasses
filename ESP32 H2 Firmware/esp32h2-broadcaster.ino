// ESP32-H2-MINI-1 USB broadcaster for band-lighthshow.
//
// The host writes color frames to this module's native USB Serial/JTAG port
// (VID 0x303A, PID 0x1001). This sketch transmits each frame on 802.15.4 so
// the MRF glasses can hear it in place of the FTDI + XBee broadcaster.
//
// Board: ESP32H2 Dev Module
// USB CDC On Boot: Enabled
// Zigbee Mode: Disabled
// Port: the module USB connector (GPIO26/GPIO27 on the MINI-1). Do not leave
//       the Arduino serial monitor open while the host app has the port.
//
// USB frame, same layout as `esp32h2::frame` in the Rust host:
//   A5 5A 60 | 96 RGB bytes (channel i at offset i*3) | XOR of those 96 bytes
//
// On air, channel 12 and PAN 2015 match the glasses receivers. Destination is
// the broadcast short address, with no ACK request, so a field of glasses does
// not answer. Change kChannel / kPanId here if the XBee was configured differently.

#include <Arduino.h>
#include <string.h>

#include "esp_ieee802154.h"
#include "esp_ieee802154_types.h"

// Present in the IEEE 802.15.4 library, but not in this core's public header.
// A weak declaration stays null if a later core drops the symbol.
extern "C" {
esp_err_t esp_ieee802154_set_auto_ack_rx(bool enable) __attribute__((weak));
esp_err_t esp_ieee802154_set_auto_ack_tx(bool enable) __attribute__((weak));
}

static const uint8_t kChannel = 12;
static const uint16_t kPanId = 2015;
static const uint16_t kSrcAddr = 0x4201;
static const uint16_t kDestAddr = 0xFFFF;
static const uint8_t kMacHeaderBytes = 9;
static const uint8_t kFcsBytes = 2;
static const uint8_t kPayloadBytes = 96;
static const uint8_t kSync0 = 0xA5;
static const uint8_t kSync1 = 0x5A;
static const int8_t kTxPowerDbm = 20;

static volatile bool txIdle = true;
static bool radioReady = false;
static bool callbacksRegistered = false;
static bool radioEnabled = false;

static uint8_t pending[kPayloadBytes];
static bool havePending = false;
static uint8_t radioFrame[128];
static uint8_t seq = 0;

static uint8_t parseState = 0;
static uint8_t parseIndex = 0;
static uint8_t parseSum = 0;
static uint8_t parsePayload[kPayloadBytes];

extern "C" void IRAM_ATTR onTxDone(
    const uint8_t *frame,
    const uint8_t *ack,
    esp_ieee802154_frame_info_t *ackInfo) {
  (void)frame;
  (void)ack;
  (void)ackInfo;
  txIdle = true;
}

extern "C" void IRAM_ATTR onTxFailed(const uint8_t *frame, esp_ieee802154_tx_error_t error) {
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
  if (esp_ieee802154_set_channel(kChannel) != ESP_OK) {
    return false;
  }
  if (esp_ieee802154_set_panid(kPanId) != ESP_OK) {
    return false;
  }
  if (esp_ieee802154_set_short_address(kSrcAddr) != ESP_OK) {
    return false;
  }
  esp_ieee802154_set_txpower(kTxPowerDbm);
  return true;
}

static void armFrame(const uint8_t *rgb) {
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
  memcpy(&radioFrame[10], rgb, kPayloadBytes);
}

static void accept(uint8_t byte) {
  switch (parseState) {
    case 0:
      if (byte == kSync0) {
        parseState = 1;
      }
      break;
    case 1:
      if (byte == kSync1) {
        parseState = 2;
      } else {
        parseState = byte == kSync0 ? 1 : 0;
      }
      break;
    case 2:
      if (byte == kPayloadBytes) {
        parseState = 3;
        parseIndex = 0;
        parseSum = 0;
      } else {
        parseState = byte == kSync0 ? 1 : 0;
      }
      break;
    case 3:
      parsePayload[parseIndex++] = byte;
      parseSum ^= byte;
      if (parseIndex == kPayloadBytes) {
        parseState = 4;
      }
      break;
    default:
      if (byte == parseSum) {
        memcpy(pending, parsePayload, kPayloadBytes);
        havePending = true;
      }
      parseState = 0;
      break;
  }
}

static void ingest() {
  while (Serial.available() > 0) {
    int value = Serial.read();
    if (value < 0) {
      return;
    }
    accept((uint8_t)value);
  }
}

static void transmitPending() {
  if (!havePending || !txIdle) {
    return;
  }
  armFrame(pending);
  havePending = false;
  txIdle = false;
  if (esp_ieee802154_transmit(radioFrame, false) != ESP_OK) {
    txIdle = true;
  }
}

void setup() {
  Serial.begin(115200);
  radioReady = startRadio();
}

void loop() {
  if (!radioReady) {
    radioReady = startRadio();
    delay(200);
    return;
  }
  ingest();
  transmitPending();
}
