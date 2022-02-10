// Signal K application template file.
//
// This application demonstrates core SensESP concepts in a very
// concise manner. You can build and upload the application as is
// and observe the value changes on the serial port monitor.
//
// You can use this source file as a basis for your own projects.
// Remove the parts that are not relevant to you, and add your own code
// for external hardware libraries.

#include <WiFi.h>

#include <list>
#include <memory>

#include "NMEA2000_CAN.h"
#include "Seasmart.h"
#include "n2k_nmea0183_transform.h"
#include "sensesp/net/http_server.h"
#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp_minimal_app_builder.h"

using namespace sensesp;

constexpr gpio_num_t kCanRxPin = GPIO_NUM_34;
constexpr gpio_num_t kCanTxPin = GPIO_NUM_32;

#define MAX_NMEA2000_MESSAGE_SEASMART_SIZE 500
#define MAX_NMEA0183_MESSAGE_SIZE 100

const size_t MaxClients = 10;

const uint16_t server_port =
    2222;  // Define the port, where served sends data. Use this e.g. on OpenCPN

// Set the information for other bus devices, which messages we support
const unsigned long transmit_messages[] PROGMEM = {0};
const unsigned long receive_messages[] PROGMEM = {
    /*126992L,*/  // System time
    127250L,      // Heading
    127258L,      // Magnetic variation
    128259UL,     // Boat speed
    128267UL,     // Depth
    129025UL,     // Position
    129026L,      // COG and SOG
    129029L,      // GNSS
    130306L,      // Wind
    0};

using WiFiClientPtr = std::shared_ptr<WiFiClient>;
std::list<WiFiClientPtr> clients;

tNMEA2000 *nmea2000;

WiFiServer server(server_port, MaxClients);

reactesp::ReactESP app;

uint32_t GetBoardSerialNumber() {
  uint8_t chipid[6];
  esp_efuse_mac_get_default(chipid);
  return chipid[0] + (chipid[1] << 8) + (chipid[2] << 16) + (chipid[3] << 24);
}

void AddClient(WiFiClient &client) {
  debugD("Registering a new client connection");
  clients.push_back(WiFiClientPtr(new WiFiClient(client)));
}

void StopClient(std::list<WiFiClientPtr>::iterator &it) {
  debugD("Client disconnected");
  (*it)->stop();
  it = clients.erase(it);
}

void CheckConnections() {
  WiFiClient client = server.available();  // listen for incoming clients

  if (client) AddClient(client);

  for (auto it = clients.begin(); it != clients.end(); it++) {
    if ((*it) != NULL) {
      if (!(*it)->connected()) {
        StopClient(it);
      } else {
        if ((*it)->available()) {
          char c = (*it)->read();
          if (c == 0x03) StopClient(it);  // Close connection by ctrl-c
        }
      }
    } else {
      it = clients.erase(it);  // Should have been erased by StopClient
    }
  }
}

void SendBufToClients(const char *buf) {
  for (auto it = clients.begin(); it != clients.end(); it++) {
    if ((*it) != NULL && (*it)->connected()) {
      (*it)->println(buf);
    }
  }
}

// NMEA 2000 message handler
void HandleNMEA2000Msg(const tN2kMsg &n2k_msg) {
  char buf[MAX_NMEA2000_MESSAGE_SEASMART_SIZE];
  if (N2kToSeasmart(n2k_msg, millis(), buf,
                    MAX_NMEA2000_MESSAGE_SEASMART_SIZE) == 0)
    return;
  Serial.println(buf);
  SendBufToClients(buf);
}

String GetSeaSmartString(const tN2kMsg &n2k_msg) {
  char buf[MAX_NMEA2000_MESSAGE_SEASMART_SIZE];
  if (N2kToSeasmart(n2k_msg, millis(), buf,
                    MAX_NMEA2000_MESSAGE_SEASMART_SIZE) == 0) {
    return "";
  } else {
    return String(buf);
  }
}

ObservableValue<tN2kMsg> n2k_msg_input;

void InitNMEA2000() {
  nmea2000->SetN2kCANMsgBufSize(8);
  nmea2000->SetN2kCANReceiveFrameBufSize(100);
  //  nmea2000->SetForwardStream(&Serial);  // PC output on due native port
  //  nmea2000->SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text
  // nmea2000->EnableForward(false);                 // Disable all msg
  // forwarding to USB (=Serial)

  char serial_number_str[33];
  uint32_t serial_number = GetBoardSerialNumber();
  snprintf(serial_number_str, 32, "%lu", (long unsigned int)serial_number);

  nmea2000->SetProductInformation(
      serial_number_str,       // Manufacturer's Model serial code
      130,                     // Manufacturer's product code
      "N2k->NMEA0183 WiFi",    // Manufacturer's Model ID
      "1.0.0.1 (2018-04-08)",  // Manufacturer's Software version code
      "1.0.0.0 (2018-04-08)"   // Manufacturer's Model version
  );
  // Det device information
  nmea2000->SetDeviceInformation(
      serial_number,  // Unique number. Use e.g. Serial number.
      130,            // Device function=PC Gateway. See codes on
            // http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20%26%20function%20codes%20v%202.00.pdf
      25,  // Device class=Inter/Intranetwork Device. See codes on
           // http://www.nmea.org/Assets/20120726%20nmea%202000%20class%20%26%20function%20codes%20v%202.00.pdf
      2046  // Just choosen free from code list on
            // http://www.nmea.org/Assets/20121020%20nmea%202000%20registration%20list.pdf
  );

  nmea2000->SetForwardType(
      tNMEA2000::fwdt_Text);  // Show in clear text. Leave uncommented for
                              // default Actisense format.
  nmea2000->SetMode(tNMEA2000::N2km_ListenAndNode, 32);
  // nmea2000->EnableForward(false);

  nmea2000->ExtendTransmitMessages(transmit_messages);
  nmea2000->ExtendReceiveMessages(receive_messages);
  nmea2000->SetMsgHandler(
      [](const tN2kMsg &n2k_msg) { n2k_msg_input.set(n2k_msg); });

  nmea2000->Open();
}

// The setup function performs one-time application initialization.
void setup() {
#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif

  SensESPMinimalAppBuilder builder;
  SensESPMinimalApp *sensesp_app =
      builder.set_hostname("sensesp-wifi-gw")->get_app();

  // Initialize the NMEA2000 library
  nmea2000 = new tNMEA2000_esp32(kCanTxPin, kCanRxPin);

  debugD("Initializing NMEA2000...");

  InitNMEA2000();

  // send the NMEA 2000 message in SeaSmart format
  n2k_msg_input.connect_to(
      new LambdaConsumer<tN2kMsg>([](const tN2kMsg &n2k_msg) {
        String msg = GetSeaSmartString(n2k_msg);
        if (msg.length() > 0) {
          Serial.println(msg);
          SendBufToClients(msg.c_str());
        }
      }));

  auto n2k_transform = new N2KTo0183Transform();

  // the message handler called within this consumer will write its output
  // to nmea0183_msg_observable
  n2k_msg_input.connect_to(n2k_transform);

  // send the generated NMEA 0183 message
  n2k_transform->connect_to(
      new LambdaConsumer<String>([](const String &msg_str) {
        Serial.println(msg_str);
        SendBufToClients(msg_str.c_str());
      }));

  auto *networking = new Networking(
      "/system/net", "", "", SensESPBaseApp::get_hostname(), "thisisfine");
  auto *http_server = new HTTPServer();

  // Here, we'll do a trick. WiFiServer needs to be instantiated after the
  // network is initialized, but that happens asynchronously after the app
  // starts. We'll create a Lambda consumer to listen to the network status
  // messages and then create the WiFiServer when the network is ready.

  networking->connect_to(new LambdaConsumer<WifiState>([](WifiState state) {
    if (state == WifiState::kWifiConnectedToAP) {
      debugD("Initializing WiFi server...");
      // FIXME: What happens if the WiFi state is temporarily disconnected?
      server.begin();
    }
  }));

  // Handle incoming connections
  app.onRepeat(1, []() { CheckConnections(); });

  // Handle incoming NMEA 2000 messages
  app.onRepeat(1, []() {
    nmea2000->ParseMessages();
  });

  // app.onAvailable(Serial, []() {
  //   // Flush the incoming serial buffer
  //   while (Serial.available()) {
  //     Serial.read();
  //   }
  // });

  sensesp_app->start();
}

void loop() { app.tick(); }
