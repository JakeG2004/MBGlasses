/*
 * uiGC0 port for the ESP32-H2-DevKitM-1.
 * Receives the 2015 show frames on the onboard 802.15.4 radio and
 * paints two 34-pixel NeoPixel strips (D6 and D7). The AVR sketch is unchanged.
 *
 * Two display modes (isSerpentine):
 *   true  - pixel i shows show channel i (spatial rainbow along the strip)
 *   false - every pixel shows show channel kShowChannelId (original behavior)
 *
 * Frames with fewer channels than the strip are accepted: missing pixels
 * repeat the last channel received.
 *
 * Board: ESP32H2 Dev Module, ESP32 Arduino core 3.x.
 * Library: Adafruit NeoPixel (1.12 or newer, uses RMT on the H2).
 * USB CDC On Boot must be Enabled. This DevKit's USB port is the chip's
 * USB-Serial/JTAG, not a UART bridge. Zigbee mode must stay off.
 *
 * Benjamin Jeffery, University of Idaho, 10/09/2015 (original)
 */

#include "esp_ieee802154.h"
#include <Adafruit_NeoPixel.h>
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

static const uint8_t kChannel = 12;      // 802.15.4 radio channel
static const uint16_t kPanId = 2015;
static const uint16_t kShortAddr = 0x4202;
static const uint8_t kShowChannelId = 1;  // single-color mode: RGB lives at id * 3 in the payload
static const uint8_t kStripPinA = D6;     // GPIO7
static const uint8_t kStripPinB = D7;     // GPIO8 (also the onboard LED pin, now unused)
static const uint16_t kStripLen = 34;     // LEDs per strip
static const uint8_t kBrightness = 255;   // 0-255, scales the received colors
static const uint8_t kMacHeaderBytes = 9;
// PHR length still counts the FCS. The H2 replaces those two bytes with RSSI
// and LQI, so they are not payload. Set this to 0 only if a live dump is
// exactly 2 bytes shorter than a frame that includes the FCS.
static const uint8_t kFcsBytes = 2;
static const uint8_t kMaxFrameBytes = 128;
static const uint32_t kLogPeriodMs = 1000;

static const uint16_t kBootPulseMs = 250;

// true = per-pixel rainbow, false = whole strip one color.
static const bool isSerpentine = false;

static const uint8_t channelId = kShowChannelId;
static const uint8_t startId = kShowChannelId * 3;

static Adafruit_NeoPixel stripA(kStripLen, kStripPinA, NEO_GRB + NEO_KHZ800);
static Adafruit_NeoPixel stripB(kStripLen, kStripPinB, NEO_GRB + NEO_KHZ800);

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

// Both strips show the same color.
static void setAll(uint8_t red, uint8_t green, uint8_t blue) {
  uint32_t color = Adafruit_NeoPixel::Color(red, green, blue);
  stripA.fill(color);
  stripB.fill(color);
  stripA.show();
  stripB.show();
}

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

// Copy every pixel this frame carries (up to kStripLen) into the pending
// arrays. Uses this frame's PHR only; never reuse a previous packet's length.
// IRAM because it is called from the radio ISR.
static bool IRAM_ATTR storeFrame(const uint8_t *frame) {
  uint8_t psduLen = frame[0] & 0x7f;
  if (psduLen < kMacHeaderBytes + kFcsBytes) {
    return false;
  }
  uint16_t n = (uint16_t)(psduLen - kMacHeaderBytes - kFcsBytes) / 3;
  if (n > kStripLen) {
    n = kStripLen;
  }
  if (n <= kShowChannelId) {
    return false;  // does not even reach the single-color channel
  }
  uint16_t idx = 1u + kMacHeaderBytes;
  if (idx + n * 3 > kMaxFrameBytes) {
    return false;
  }

  frameSeq++;  // odd: write in progress
  for (uint16_t i = 0; i < n; i++) {
    pendingRArr[i] = frame[idx++];
    pendingGArr[i] = frame[idx++];
    pendingBArr[i] = frame[idx++];
  }
  pendingCount = (uint8_t)n;
  frameSeq++;  // even: stable
  return true;
}

// ISR context. Copy only; the strips and Serial stay in loop().
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

    if (storeFrame(frame)) {
      colorPending = true;
    } else {
      rxShortCount++;
    }
  }
  esp_ieee802154_receive_handle_done(frame);
}

// A failed receive must not clear the strips or the pending color.
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
  Serial.print(" mode ");
  Serial.print(isSerpentine ? "strip" : "single");
  Serial.print(" px ");
  Serial.print(pendingCount);
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

  stripA.begin();
  stripB.begin();
  stripA.setBrightness(kBrightness);
  stripB.setBrightness(kBrightness);
  setAll(0, 0, 0);

  // Visible proof the flash took, kept dim: 68 pixels at full white is ~4 A.
  // Off until a valid frame arrives.
#if !ARDUINO_USB_CDC_ON_BOOT
  setAll(40, 0, 0);
  delay(200);
  setAll(0, 0, 0);
  delay(100);
#endif
  setAll(40, 40, 40);
  delay(kBootPulseMs);
  setAll(0, 0, 0);

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

  uint8_t redArr[kStripLen] = {0};
  uint8_t greenArr[kStripLen] = {0};
  uint8_t blueArr[kStripLen] = {0};

  if (colorPending) {
    colorPending = false;

    // Consistent snapshot of the latest frame (see frameSeq above).
    uint32_t s1, s2;
    uint8_t count;
    do {
      s1 = frameSeq;
      count = pendingCount;
      for (int i = 0; i < kStripLen; i++) {
        redArr[i] = pendingRArr[i];
        greenArr[i] = pendingGArr[i];
        blueArr[i] = pendingBArr[i];
      }
      s2 = frameSeq;
    } while ((s1 & 1) || s1 != s2);

    // Short frame: repeat the last pixel received across the rest of the strip.
    if (count > 0 && count < kStripLen) {
      for (int i = count; i < kStripLen; i++) {
        redArr[i] = redArr[count - 1];
        greenArr[i] = greenArr[count - 1];
        blueArr[i] = blueArr[count - 1];
      }
    }

    red = redArr[kShowChannelId];
    green = greenArr[kShowChannelId];
    blue = blueArr[kShowChannelId];
    haveColor = true;
  }

  if (haveColor) {
    if (isSerpentine) {
      setStrip(redArr, greenArr, blueArr);
    } else {
      setAll(red, green, blue);
    }
  } else if (!radioOk) {
    setAll(40, 0, 0);
    delay(30);
    setAll(0, 0, 0);
  }

  if (dumpPending) {
    dumpPending = false;
    dumpFirstFrame();
  }

  logStatus(haveColor, red, green, blue);
}
