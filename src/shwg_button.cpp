#include "AceButton.h"
#include "config.h"
#include "elapsedMillis.h"
#include "sensesp.h"
#include "shwg.h"

using namespace ace_button;
using namespace sensesp;

// define the button control interface
static AceButton *hall_button;

static void HandleButtonEvent(AceButton *button, uint8_t event_type,
                       uint8_t button_state) {
  digitalWrite(kRedLedPin, button_state);
  static elapsedMillis time_since_press_event;

  switch (event_type) {
    case AceButton::kEventPressed:
      time_since_press_event = 0;
      break;
    case AceButton::kEventLongReleased:
      debugD("Long release, duration: %d", time_since_press_event);
      if (time_since_press_event > 10000) {
        debugD("Factory reset");
        sensesp_app->reset();
      } else if (time_since_press_event > 1000) {
        debugD("Reset");
        ESP.restart();
      }
      break;
    default:
      break;
  }
}

/**
 * @brief Setup hall effect sensor button handler.
 */
void SetupButton() {
  // set up the hall effect sensor button interface

  hall_button = new AceButton(kHallInputPin);
  pinMode(kHallInputPin, INPUT_PULLUP);
  hall_button->setEventHandler(HandleButtonEvent);

  // enable long press events
  ButtonConfig *button_config = hall_button->getButtonConfig();
  button_config->setFeature(ButtonConfig::kFeatureLongPress);
  button_config->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);

  app.onRepeat(4, []() { hall_button->check(); });
}
