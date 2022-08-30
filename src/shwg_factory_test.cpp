#include "shwg_factory_test.h"

#include "AceButton.h"
#include "WiFi.h"
#include "config.h"
#include "elapsedMillis.h"
#include "firmware_info.h"
#include "sensesp.h"
#include "shwg.h"

using namespace ace_button;
using namespace sensesp;

static constexpr char kExpectedNetworkName[] = "Hat Labs Sensors";

/**
 * @brief Check if the factory test procedure should be launched.
 *
 * If CAN RX and TX are both pulled down at device boot, the factory test
 * procedure should be launched.
 *
 * @return true
 * @return false
 */
bool FactoryTestRequested() {
  pinMode(kCanRxPin, INPUT_PULLUP);
  pinMode(kCanTxPin, INPUT_PULLUP);

  bool rx_pulled_down = digitalRead(kCanRxPin) == LOW;
  bool tx_pulled_down = digitalRead(kCanTxPin) == LOW;

  if (rx_pulled_down && tx_pulled_down) {
    debugI("Factory test requested");
    return true;
  } else {
    return false;
  }
}

static void ScanWiFiNetworks() {
  debugD("Scanning WiFi networks");
  int n = WiFi.scanNetworks();
  debugD("Network scan finished");
  if (n == 0) {
    debugE("No WiFi networks found.");
    app.onDelay(1000, ScanWiFiNetworks);
    // turn the blued LED off
    digitalWrite(kBlueLedPin, LOW);
  } else {
    // we've found some networks, check if the test network is there
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == kExpectedNetworkName) {
        debugI("Found WiFi network: %s", WiFi.SSID(i).c_str());
        int32_t rssi = WiFi.RSSI(i);
        debugI("RSSI: %d", rssi);
        if (rssi > -60) {
          // make blue LED blink rapidly
          ledcSetup(kBluePWMChannel, 8, 8);
          ledcAttachPin(kBlueLedPin, kBluePWMChannel);
          ledcWrite(kBluePWMChannel, 127);  // 50% duty cycle
          debugI("Waiting for magnet test");
          return;
        }
      }
    }
    debugE("Test WiFi network not found or RSSI too low.");
    app.onDelay(500, ScanWiFiNetworks);
  }
}

static void PrepareWiFiNetworkScan() {
  debugD("Preparing WiFi network scan");
  // Enable WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  app.onDelay(100, ScanWiFiNetworks);
}

static void HandleFactoryTestButtonEvent(AceButton* button, uint8_t event_type,
                                         uint8_t button_state) {
  digitalWrite(kRedLedPin, button_state);
  static elapsedMillis time_since_press_event;

  switch (event_type) {
    case AceButton::kEventPressed:
      time_since_press_event = 0;
      break;
    case AceButton::kEventLongReleased:
      debugD("Long release, duration: %d", time_since_press_event);
      if (time_since_press_event > 1000) {
        debugD("Detected a long press");
        // stop yellow LED blinking and turn it on
        ledcWrite(kYellowPWMChannel, 255);
        PrintProductInfo();
      }
      break;
  }
}

static AceButton* hall_button;

static void SetupFactoryTestButton() {
  debugD("Setting up factory test button handler");
  hall_button = new AceButton(kHallInputPin);
  pinMode(kHallInputPin, INPUT_PULLUP);
  hall_button->setEventHandler(HandleFactoryTestButtonEvent);

  // enable long press events
  ButtonConfig* button_config = hall_button->getButtonConfig();
  button_config->setFeature(ButtonConfig::kFeatureLongPress);
  button_config->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);

  app.onRepeat(4, []() { hall_button->check(); });
}

void SetupFactoryTest() {
  debugD("Setting up factory test mode");
  // Make the yellow LED blink as a sign of the factory test mode
  ledcSetup(kYellowPWMChannel, 4, 8);
  pinMode(kYellowLedPin, OUTPUT);
  ledcAttachPin(kYellowLedPin, kYellowPWMChannel);
  // blink at 50% duty cycle
  ledcWrite(kYellowPWMChannel, 127);

  // turn the red, blue, and CAN LEDs on
  pinMode(kRedLedPin, OUTPUT);
  pinMode(kBlueLedPin, OUTPUT);
  pinMode(kCanLedEnPin, OUTPUT);

  digitalWrite(kRedLedPin, HIGH);
  digitalWrite(kBlueLedPin, HIGH);
  digitalWrite(kCanLedEnPin, HIGH);

  SetupFactoryTestButton();

  app.onDelay(1000, PrepareWiFiNetworkScan);
}
