/*
 * uiGC0 port for the ESP32-H2-DevKitM-1.
 * Receives the 2015 show frames on the onboard 802.15.4 radio and
 * paints the DevKit RGB LED on GPIO8. The AVR sketch is unchanged.
 *
 * Board: ESP32H2 Dev Module, ESP32 Arduino core 3.x.
 * USB CDC On Boot must be Enabled. This DevKit's USB port is the chip's
 * USB-Serial/JTAG, not a UART bridge. Zigbee mode must stay off.
 *
 * Benjamin Jeffery, University of Idaho, 10/09/2015 (original)
 */

#include "esp_ieee802154.h"
#include <string.h>

// The DevKit USB connector is the chip's USB-Serial/JTAG. With CDC off,
// Serial is UART0 and the monitor on that port stays blank.
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#warning "USB CDC On Boot is disabled. Enable it or the USB monitor stays blank."
#endif

// Present since ESP-IDF 5.1, but not in the public header. Promiscuous mode
// already clears them; call them so a later core cannot leave ACK enabled.
extern "C" {
esp_err_t esp_ieee802154_set_auto_ack_rx(bool enable) __attribute__((weak));
esp_err_t esp_ieee802154_set_auto_ack_tx(bool enable) __attribute__((weak));
void esp_ieee802154_receive_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info);
}

static const uint8_t kChannel = 12;
static const uint16_t kPanId = 2015;
static const uint16_t kShortAddr = 0x4202;
static const uint8_t kDefaultChannelId = 0;
static const bool kUseDipSwitch = false;
static const uint8_t kDipPins[] = {10, 11, 12, 22};
static const uint8_t kLedPin = 8;
static const uint8_t kMacHeaderBytes = 9;
// PHR length still counts the FCS. The H2 replaces those two bytes with RSSI
// and LQI, so they are not payload. Set this to 0 only if a live dump is
// exactly 2 bytes shorter than a frame that includes the FCS.
static const uint8_t kFcsBytes = 2;
static const uint8_t kMaxFrameBytes = 128;
static const uint32_t kLogPeriodMs = 1000;

static const uint16_t kBootPulseMs = 250;

static uint8_t channelId = kDefaultChannelId;
static uint8_t startId = 0;

static volatile bool colorPending = false;
static volatile uint8_t pendingR = 0;
static volatile uint8_t pendingG = 0;
static volatile uint8_t pendingB = 0;
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

// Active-low, same as ~PINC & 0x0f. Open pins read high and contribute 0.
static uint8_t readChannelId() {
  if (!kUseDipSwitch) {
    return kDefaultChannelId;
  }

  uint8_t bits = 0;
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(kDipPins[i], INPUT_PULLUP);
    if (digitalRead(kDipPins[i]) == LOW) {
      bits |= (1 << i);
    }
  }
  return bits & 0x0f;
}

static void setLed(uint8_t red, uint8_t green, uint8_t blue) {
  neopixelWrite(kLedPin, red, green, blue);
}

// No coordinator, no ACK, no Zigbee, no Thread. Promiscuous receive only.
// Register before enable. This core calls the list, and only falls back to
// the weak symbol when that pointer is null.
static bool startRadio() {
  esp_ieee802154_event_cb_list_t callbacks = {};
  callbacks.rx_done_cb = esp_ieee802154_receive_done;
  if (esp_ieee802154_event_callback_list_register(callbacks) != ESP_OK) {
    return false;
  }
  if (esp_ieee802154_enable() != ESP_OK) {
    return false;
  }
  esp_ieee802154_set_coordinator(false);

  if (esp_ieee802154_set_channel(kChannel) != ESP_OK) {
    return false;
  }
  if (esp_ieee802154_set_panid(kPanId) != ESP_OK) {
    return false;
  }
  if (esp_ieee802154_set_short_address(kShortAddr) != ESP_OK) {
    return false;
  }
  if (esp_ieee802154_set_promiscuous(true) != ESP_OK) {
    return false;
  }
  if (esp_ieee802154_set_auto_ack_rx != nullptr) {
    esp_ieee802154_set_auto_ack_rx(false);
  }
  if (esp_ieee802154_set_auto_ack_tx != nullptr) {
    esp_ieee802154_set_auto_ack_tx(false);
  }
  if (esp_ieee802154_set_rx_when_idle(true) != ESP_OK) {
    return false;
  }
  return esp_ieee802154_receive() == ESP_OK;
}

static void rearmReceive() {
  if (!radioOk) {
    return;
  }
  esp_ieee802154_state_t state = esp_ieee802154_get_state();
  if (state != ESP_IEEE802154_RADIO_RECEIVE && state != ESP_IEEE802154_RADIO_TRANSMIT) {
    esp_ieee802154_receive();
  }
}

// This frame's PHR only. Do not reuse a previous packet's length.
static bool rgbFromFrame(const uint8_t *frame, uint8_t *red, uint8_t *green, uint8_t *blue) {
  uint8_t psduLen = frame[0] & 0x7f;
  uint16_t need = (uint16_t)kMacHeaderBytes + startId + 3 + kFcsBytes;
  if (need > psduLen) {
    return false;
  }

  uint16_t rgbIndex = 1u + kMacHeaderBytes + startId;
  if (rgbIndex + 2 >= kMaxFrameBytes) {
    return false;
  }

  *red = frame[rgbIndex];
  *green = frame[rgbIndex + 1];
  *blue = frame[rgbIndex + 2];
  return true;
}

// ISR context. Copy only; the LED and Serial stay in loop().
extern "C" void IRAM_ATTR esp_ieee802154_receive_done(uint8_t *frame, esp_ieee802154_frame_info_t *frame_info) {
  if (frame != nullptr) {
    rxCount++;

    if (!firstFrameSaved) {
      uint8_t copyLen = (frame[0] & 0x7f) + 1;
      if (copyLen > kMaxFrameBytes) {
        copyLen = kMaxFrameBytes;
      }
      for (uint8_t i = 0; i < copyLen; i++) {
        firstFrame[i] = frame[i];
      }
      firstFrameLen = copyLen;
      if (frame_info != nullptr) {
        firstRssi = frame_info->rssi;
        firstLqi = frame_info->lqi;
      }
      firstFrameSaved = true;
      dumpPending = true;
    }

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;
    if (rgbFromFrame(frame, &red, &green, &blue)) {
      pendingR = red;
      pendingG = green;
      pendingB = blue;
      colorPending = true;
    } else {
      rxShortCount++;
    }
  }
  esp_ieee802154_receive_handle_done(frame);
}

// A failed receive must not clear the LED or the pending color.
extern "C" void esp_ieee802154_receive_failed(uint16_t error) {
  (void)error;
  rxFailCount++;
}

static void dumpFirstFrame() {
  Serial.print("first psdu ");
  Serial.print(firstFrameLen > 0 ? firstFrame[0] : 0);
  Serial.print(" need ");
  Serial.print(kMacHeaderBytes + startId + 3 + kFcsBytes);
  Serial.print(" fcs ");
  Serial.print(kFcsBytes);
  Serial.print(" rssi ");
  Serial.print(firstRssi);
  Serial.print(" lqi ");
  Serial.print(firstLqi);
  Serial.print(" rgb@");
  Serial.println(1 + kMacHeaderBytes + startId);

  for (uint8_t i = 0; i < firstFrameLen; i++) {
    if (firstFrame[i] < 16) {
      Serial.print('0');
    }
    Serial.print(firstFrame[i], HEX);
    if ((i & 0x0f) == 0x0f || i + 1 == firstFrameLen) {
      Serial.println();
    } else {
      Serial.print(' ');
    }
  }
}

static const char *radioStateName(esp_ieee802154_state_t state) {
  switch (state) {
    case ESP_IEEE802154_RADIO_DISABLE:
      return "off";
    case ESP_IEEE802154_RADIO_IDLE:
      return "idle";
    case ESP_IEEE802154_RADIO_SLEEP:
      return "sleep";
    case ESP_IEEE802154_RADIO_RECEIVE:
      return "rx";
    case ESP_IEEE802154_RADIO_TRANSMIT:
      return "tx";
    default:
      return "?";
  }
}

// Always prints. A quiet sketch looks like a dead USB port once the banner
// has scrolled past the reset.
static void logStatus(bool haveColor, uint8_t red, uint8_t green, uint8_t blue) {
  static uint32_t lastLogMs = 0;
  static uint8_t shownR = 0;
  static uint8_t shownG = 0;
  static uint8_t shownB = 0;
  static bool shown = false;

  if (haveColor) {
    shownR = red;
    shownG = green;
    shownB = blue;
    shown = true;
  }

  uint32_t now = millis();
  if ((uint32_t)(now - lastLogMs) < kLogPeriodMs) {
    return;
  }
  lastLogMs = now;

  uint32_t seen = 0;
  uint32_t shorts = 0;
  uint32_t fails = 0;
  noInterrupts();
  seen = rxCount;
  shorts = rxShortCount;
  fails = rxFailCount;
  interrupts();

  Serial.print(radioOk ? "listening" : "radio failed");
  Serial.print(" id ");
  Serial.print(channelId);
  Serial.print(" rx ");
  Serial.print(seen);
  Serial.print(" short ");
  Serial.print(shorts);
  Serial.print(" fails ");
  Serial.print(fails);
  Serial.print(" state ");
  Serial.print(radioStateName(esp_ieee802154_get_state()));
  if (shown) {
    Serial.print(" rgb ");
    Serial.print(shownR);
    Serial.print(' ');
    Serial.print(shownG);
    Serial.print(' ');
    Serial.print(shownB);
  }
  Serial.println();
}

static void waitForHost() {
#if ARDUINO_USB_CDC_ON_BOOT
  uint32_t start = millis();
  while (!Serial && (uint32_t)(millis() - start) < 2500) {
    delay(20);
  }
  delay(50);
#endif
}

void setup() {
  Serial.begin(115200);

  channelId = readChannelId();
  startId = channelId * 3;

  // Visible proof the flash took. Off until a valid frame arrives.
#if !ARDUINO_USB_CDC_ON_BOOT
  setLed(255, 0, 0);
  delay(200);
  setLed(0, 0, 0);
  delay(100);
#endif
  setLed(255, 255, 255);
  delay(kBootPulseMs);
  setLed(0, 0, 0);

  waitForHost();

  Serial.print("uiGC0 H2 radio ch ");
  Serial.print(kChannel);
  Serial.print(" pan ");
  Serial.print(kPanId);
  Serial.print(" addr 0x");
  Serial.print(kShortAddr, HEX);
  Serial.print(" id ");
  Serial.print(channelId);
  Serial.print(" offset ");
  Serial.println(startId);

  radioOk = startRadio();
  if (!radioOk) {
    Serial.println("radio start failed");
  } else {
    Serial.println("listening");
  }
}

void loop() {
  rearmReceive();

  bool haveColor = false;
  uint8_t red = 0;
  uint8_t green = 0;
  uint8_t blue = 0;
  noInterrupts();
  if (colorPending) {
    red = pendingR;
    green = pendingG;
    blue = pendingB;
    colorPending = false;
    haveColor = true;
  }
  interrupts();

  if (haveColor) {
    setLed(red, green, blue);
  } else if (!radioOk) {
    setLed(180, 0, 0);
    delay(30);
    setLed(0, 0, 0);
  }

  if (dumpPending) {
    dumpPending = false;
    dumpFirstFrame();
  }

  logStatus(haveColor, red, green, blue);
}
