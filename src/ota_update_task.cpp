#include "ota_update_task.h"

#include <esp_task_wdt.h>

#include <HTTPClient.h>
#include <HttpsOTAUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "ReactESP.h"
#include "firmware_info.h"

/**
 * How long to wait before the next check if no firmware update was available.
 */
static constexpr int kDelayBetweenFirmwareUpdateChecksMs = 60 * 60 * 1000;  // 1 hour

/**
 * How long to wait before retrying if the server could not be reached.
 */
static constexpr int kDelayAfterFailedHTTPConnectionMs = 60 * 1000;  // 1 minute

/**
 * How long to wait if the server has returned with an erroneous HTTP status code.
 */
static constexpr int kDelayAfterHTTPErrorMs = 10 * 60 * 1000;  // 10 minutes

/**
 * How long to wait before retrying if the WiFi connection is down.
 */
static constexpr int kDelayAfterFailedWiFiConnectionMs = 10 * 1000;  // 10 s

static const char* server_ca_certificate =
    "-----BEGIN CERTIFICATE-----\n"
    "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\n"
    "-----END CERTIFICATE-----\n";

static uint32_t latest_version = -1;

static ReactESP* task_app;

// a forward declaration is needed to resolve a circular dependency
static void CheckWiFiSTA();

void OTAHttpEvent(HttpEvent_t* event) {
  switch (event->event_id) {
    case HTTP_EVENT_ERROR:
      Serial.println("Http Event Error");
      break;
    case HTTP_EVENT_ON_CONNECTED:
      Serial.println("Http Event On Connected");
      break;
    case HTTP_EVENT_HEADER_SENT:
      Serial.println("Http Event Header Sent");
      break;
    case HTTP_EVENT_ON_HEADER:
      Serial.printf("Http Event On Header, key=%s, value=%s\n",
                    event->header_key, event->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
      break;
    case HTTP_EVENT_ON_FINISH:
      Serial.println("Http Event On Finish");
      break;
    case HTTP_EVENT_DISCONNECTED:
      Serial.println("Http Event Disconnected");
      break;
  }
}

static void PerformOTAUpdate() {
  char url[128];
  String mac_address = WiFi.macAddress();
  // perform the actual update
  HttpsOTA.onHttpEvent(OTAHttpEvent);
  snprintf(
    url, sizeof(url),
    "https://ota.hatlabs.fi/%s/firmware_%08x.bin?mac=%s",
    kFirmwareName, latest_version, mac_address.c_str()
  );
  debugI("Performing OTA update from %s", url);
  HttpsOTA.begin(url, server_ca_certificate);
  debugD("OTA update started.");
}

static void CheckForUpdates() {
  WiFiClientSecure* client = new WiFiClientSecure;
  HTTPClient* https = new HTTPClient;
  client->setCACert(server_ca_certificate);
  char url[128];
  String mac_address = WiFi.macAddress();

  // try to check for available updates

  snprintf(url, sizeof(url), "https://ota.hatlabs.fi/%s/latest?mac=%s",
           kFirmwareName, mac_address.c_str());

  debugD("Establishing connection to %s", url);
  if (https->begin(*client, url)) {
    int http_code = https->GET();

    if (http_code > 0) {
      if (http_code == HTTP_CODE_OK ||
          http_code == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = https->getString();
        latest_version = strtoul(payload.c_str(), 0, 16);
        debugD("Latest available version: %08x", latest_version);
        debugD("Current version: %08x", kFirmwareHexVersion);
        // compare the available version to the current version
        if (latest_version > kFirmwareHexVersion) {
          debugI("New firmware available");
          task_app->onDelay(1, PerformOTAUpdate);
        } else {
          debugD("No new firmware available; sleeping");
          task_app->onDelay(kDelayBetweenFirmwareUpdateChecksMs, CheckWiFiSTA);
        }
      }
    } else {
      String error_string = https->errorToString(http_code);
      debugE("HTTPS GET failed, error: %s", error_string.c_str());
      task_app->onDelay(kDelayAfterHTTPErrorMs, CheckWiFiSTA);
    }
  } else {
    // if we couldn't reach the server, try again in a minute

    debugD("HTTP connection failed");
    task_app->onDelay(kDelayAfterFailedHTTPConnectionMs, CheckWiFiSTA);
  }

  delete https;
  delete client;
}

/**
 * @brief Keep waiting until WiFi is connected and is in STA mode.
 */
static void CheckWiFiSTA() {
  debugD("Checking WiFi state...");
  if (WiFi.status() == WL_CONNECTED && WiFi.getMode() == WIFI_STA) {
    // Connected and in STA mode; proceed to checking for OTA updates.
    debugD("WiFi connected and in STA mode.");
    task_app->onDelay(1, CheckForUpdates);
  } else {
    debugD("WiFi not connected or not in STA mode; waiting...");
    task_app->onDelay(kDelayAfterFailedWiFiConnectionMs, CheckWiFiSTA);
  }
}

static void CheckOTAStatus() {
  HttpsOTAStatus_t otastatus;
  static HttpsOTAStatus_t last_otastatus = HTTPS_OTA_IDLE;
  otastatus = HttpsOTA.status();
  if (otastatus == HTTPS_OTA_SUCCESS && last_otastatus != HTTPS_OTA_SUCCESS) {
    debugI("Firmware written successfully. To apply the changes, reboot the device.");
  } else if (otastatus == HTTPS_OTA_FAIL && last_otastatus != HTTPS_OTA_FAIL) {
    debugE("Firmware update failed.");
  }
  last_otastatus = otastatus;
}

void ExecuteOTAUpdateTask(void* task_args) {
  debugD("Executing OTA update task");

  // update the watchdog timer
  esp_task_wdt_init(15, 0);

  task_app = new ReactESP(false);

  // wait until WiFi is connected and in STA mode
  task_app->onDelay(1, CheckWiFiSTA);

  task_app->onRepeat(1000, CheckOTAStatus);

  while (true) {
    task_app->tick();

    // a small delay required to prevent the task watchdog from triggering
    delay(1);
  }
}
