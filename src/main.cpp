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
#include <sys/time.h>

#include <list>
#include <memory>

#include "N2kMessages.h"
#include "NMEA2000/NMEA2000_esp32_framehandler.h"
#include "NMEA2000_CAN.h"
#include "Seasmart.h"
#include "can_frame.h"
#include "config.h"
#include "firmware_info.h"
#include "n2k_nmea0183_transform.h"
#include "ota_update_task.h"
#include "sensesp/net/discovery.h"
#include "sensesp/net/http_server.h"
#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/led_blinker.h"
#include "sensesp/system/ui_output.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp_minimal_app_builder.h"
#include "shwg.h"
#include "shwg_button.h"
#include "shwg_factory_test.h"
#include "streaming_tcp_server.h"
#include "streaming_udp_server.h"
#include "time_string.h"
#include "ui_controls.h"
#include "ydwg_raw_output.h"
#include "ydwg_raw_parser.h"

using namespace sensesp;

#define MAX_NMEA2000_MESSAGE_SEASMART_SIZE 500
#define MAX_NMEA0183_MESSAGE_SIZE 200

// Set the information for other bus devices, which messages we support
const unsigned long kTransmitMessages[] PROGMEM = {0};
const unsigned long ReceiveMessages[] PROGMEM = {
    /*126992L,*/  // System time
    127250L,      // Heading
    127258L,      // Magnetic variation
    128259UL,     // Boat speed
    128267UL,     // Depth
    129025UL,     // Position
    129026L,      // COG and SOG
    129029L,      // GNSS
    130306L,      // Wind
    129038L,      // AIS Class A Position Report, Message Type 1
    129039L,      // AIS Class B Position Report, Message Type 18
    12979UL,  // AIS Class A Ship Static and Voyage related data, Message Type 5
    129809L,  // AIS Class B "CS" Static Data Report, Part A
    129810L,  // AIS Class B "CS" Static Data Report, Part B
    0};

tNMEA2000_esp32_FH *nmea2000;

StreamingTCPServer *nmea0183_tcp_server;
StreamingTCPServer *ydwg_raw_tcp_server;

StreamingUDPServer *nmea0183_udp_server;
StreamingUDPServer *ydwg_raw_udp_server;

// time elapsed since last system time update
elapsedMillis elapsed_since_last_system_time_update = kTimeUpdatePeriodMs;

reactesp::ReactESP app;

SensESPMinimalApp *sensesp_app;

Networking *networking;

CheckboxConfig *checkbox_config_enable_firmware_updates;
PortConfig *port_config_ydwg_raw_tcp_tx;
BiDiPortConfig *port_config_ydwg_raw_udp;
CheckboxConfig *checkbox_config_translate_to_seasmart;
CheckboxConfig *checkbox_config_translate_to_nmea0183;
PortConfig *port_config_nmea0183_tcp_tx;
PortConfig *port_config_nmea0183_udp_tx;

UIOutput<String> ui_output_firmware_name("Firmware name", kFirmwareName,
                                         "Firmware", 100);
UIOutput<String> ui_output_firmware_version("Firmware version",
                                            kFirmwareVersion, "Firmware", 110);
UIOutput<String> ui_output_build_info =
    UIOutput<String>("Built at", __DATE__ " " __TIME__, "Firmware", 120);

UILambdaOutput<String> ui_output_hostname = UILambdaOutput<String>(
    "Hostname", []() { return sensesp_app->get_hostname(); }, "WiFi", 200);
UILambdaOutput<String> ui_output_ip_address = UILambdaOutput<String>(
    "IP address", []() { return WiFi.localIP().toString(); }, "WiFi", 210);
UILambdaOutput<String> ui_output_mac_address = UILambdaOutput<String>(
    "MAC", []() { return WiFi.macAddress(); }, "WiFi", 220);
UILambdaOutput<String> ui_output_wifi_ssid = UILambdaOutput<String>(
    "SSID", []() { return WiFi.SSID(); }, "WiFi", 230);
UILambdaOutput<int8_t> ui_output_wifi_rssi = UILambdaOutput<int8_t>(
    "WiFi signal strength (dB)", []() { return WiFi.RSSI(); }, "WiFi", 240);

uint32_t can_frame_rx_counter = 0;
uint32_t can_frame_tx_counter = 0;

UILambdaOutput<uint32_t> ui_output_can_frame_rx_counter(
    "CAN frame RX counter", []() { return can_frame_rx_counter; }, "NMEA 2000",
    300);

UILambdaOutput<uint32_t> ui_output_can_frame_tx_counter(
    "CAN frame TX counter", []() { return can_frame_tx_counter; }, "NMEA 2000",
    310);

UILambdaOutput<int> ui_output_uptime(
    "Uptime", []() { return millis() / 1000; }, "Runtime", 400);

int led_state = -1;

uint32_t GetBoardSerialNumber() {
  uint8_t chipid[6];
  esp_efuse_mac_get_default(chipid);
  return chipid[0] + (chipid[1] << 8) + (chipid[2] << 16) + (chipid[3] << 24);
}

// Set system time if the correct PGN is received
void SetSystemTime(const tN2kMsg &n2k_msg) {
  unsigned char SID;
  uint16_t n2k_system_date;  // days since 1970-01-01
  double n2k_system_time;    // seconds since midnight
  tN2kTimeSource time_source;

  double system_timestamp;

  // If the received PGN 8s 126992 (System Time) and startup time is 0,
  // set the startup time to the received time.
  if (n2k_msg.PGN == 126992) {
    if (elapsed_since_last_system_time_update > kTimeUpdatePeriodMs) {
      debugD("Updating system time");
      if (ParseN2kSystemTime(n2k_msg, SID, n2k_system_date, n2k_system_time,
                             time_source)) {
        system_timestamp = n2k_system_date * 86400 + n2k_system_time;
        struct timeval tv;
        tv.tv_sec = system_timestamp;
        tv.tv_usec = 1e6 * (system_timestamp - tv.tv_sec);
        settimeofday(&tv, NULL);

        String time_str = GetUTCTimeString(tv);
        debugI("Set system time to %s", time_str.c_str());
        elapsed_since_last_system_time_update = 0;
      }
    }
  }
}

String GetSeaSmartString(const tN2kMsg &n2k_msg) {
  char buf[MAX_NMEA2000_MESSAGE_SEASMART_SIZE];
  if (N2kToSeasmart(n2k_msg, millis(), buf,
                    MAX_NMEA2000_MESSAGE_SEASMART_SIZE) == 0) {
    return "";
  } else {
    return String(buf) + "\r\n";
  }
}

ObservableValue<tN2kMsg> n2k_msg_input;
ObservableValue<CANFrame> can_frame_input;

class SeasmartTransform : public Transform<tN2kMsg, String> {
 public:
  SeasmartTransform() : Transform<tN2kMsg, String>() {}

  void set_input(tN2kMsg input, uint8_t input_channel = 0) override {
    String seasmart_str = GetSeaSmartString(input);
    if (seasmart_str.length() > 0) {
      this->emit(seasmart_str);
    }
  }
};

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
      serial_number_str,  // Manufacturer's Model serial code
      130,                // Manufacturer's product code
      "SH-wg",            // Manufacturer's Model ID
      kFirmwareVersion,   // Manufacturer's Software version code
      "1.0.0"             // Manufacturer's Model version
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

  nmea2000->ExtendTransmitMessages(kTransmitMessages);
  nmea2000->ExtendReceiveMessages(ReceiveMessages);

  nmea2000->SetCANFrameHandler([](bool &has_frame, unsigned long &can_id,
                                  unsigned char &len, unsigned char *buf) {
    struct CANFrame frame;
    if (has_frame) {
      frame.id = can_id;
      frame.len = len;
      memcpy(frame.buf, buf, len);
      can_frame_input.set(frame);
    }
  });
  nmea2000->SetMsgHandler(
      [](const tN2kMsg &n2k_msg) { n2k_msg_input.set(n2k_msg); });

  can_frame_input.connect_to(new LambdaConsumer<CANFrame>(
      [](CANFrame frame) { can_frame_rx_counter++; }));

  nmea2000->Open();
}

static void SetupBlueLEDBlinker() {
  // set up the PWM channel for the blue LED
  ledcSetup(kBluePWMChannel, 2, 16);

  auto wifi_state_consumer =
      new LambdaConsumer<WiFiState>([](const WiFiState state) {
        switch (state) {
          case WiFiState::kWifiNoAP:
          case WiFiState::kWifiDisconnected:
            ledcDetachPin(kBlueLedPin);
            digitalWrite(kBlueLedPin, LOW);
            break;
          case WiFiState::kWifiConnectedToAP:
            digitalWrite(kBlueLedPin, HIGH);
            break;
          case WiFiState::kWifiManagerActivated:
            ledcAttachPin(kBlueLedPin, kBluePWMChannel);
            // blink the blue LED at 2 Hz and 25% duty cycle
            ledcWrite(kBluePWMChannel, 64);
            break;
          default:
            digitalWrite(kBlueLedPin, LOW);
            break;
        }
      });

  networking->connect_to(wifi_state_consumer);
}

static void SetupYellowLEDBlinker(
    LambdaTransform<CANFrame, String> *ydwg_transform) {
  static int solid_on_pattern[] = {1000, 0, PATTERN_END};
  auto blinker = new PatternBlinker(kYellowLedPin, solid_on_pattern);

  ydwg_transform->connect_to(
      new LambdaConsumer<String>([blinker](const String &str) {
        if (WiFi.isConnected()) {
          blinker->blip(5);
        }
      }));
}

static void SetupTransmitters() {
  auto can_to_ydwg_transform =
      new LambdaTransform<CANFrame, String>([](CANFrame frame) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        String raw_string = CANFrameToYDWGRaw(frame, tv);
        return raw_string;
      });

  auto n2k_to_0183_transform = new N2KTo0183Transform();
  auto n2k_to_seasmart_transform = new SeasmartTransform();
  auto ydwg_raw_to_can_transform = new YDWGRawToCANFrameTransform();

  // if configured, connect the N2K input to NMEA 0183 transform

  if (checkbox_config_translate_to_nmea0183->get_value()) {
    // the message handler called within this consumer will write its output
    // to nmea0183_msg_observable
    debugD("Connecting N2K to NMEA 0183");
    n2k_msg_input.connect_to(n2k_to_0183_transform);
  }

  // if configured, connect the N2K input to Seasmart transform

  if (checkbox_config_translate_to_seasmart->get_value()) {
    debugD("Connecting N2K to Seasmart");
    n2k_msg_input.connect_to(n2k_to_seasmart_transform);
  }

  // set up the YDWG RAW TCP server

  debugD("Setting up YDWG RAW TCP server");
  int ydwg_raw_tcp_port = port_config_ydwg_raw_tcp_tx->get_port();
  ydwg_raw_tcp_server = new StreamingTCPServer(ydwg_raw_tcp_port, networking);
  ydwg_raw_tcp_server->set_enabled(port_config_ydwg_raw_tcp_tx->get_enabled());

  // set up the YDWG RAW UDP server

  debugD("Setting up YDWG RAW UDP server");
  int ydwg_raw_udp_port = port_config_ydwg_raw_udp->get_port();
  ydwg_raw_udp_server = new StreamingUDPServer(ydwg_raw_udp_port, networking);
  if (!port_config_ydwg_raw_udp->get_tx_enabled() &&
      !port_config_ydwg_raw_udp->get_rx_enabled()) {
    ydwg_raw_udp_server->set_enabled(false);
  }

  // set up the NMEA 0183 TCP server

  debugD("Setting up NMEA 0183 TCP server");
  int nmea0183_tcp_port = port_config_nmea0183_tcp_tx->get_port();
  nmea0183_tcp_server = new StreamingTCPServer(nmea0183_tcp_port, networking);
  nmea0183_tcp_server->set_enabled(port_config_nmea0183_tcp_tx->get_enabled());

  // set up the NMEA 0183 UDP server

  debugD("Setting up NMEA 0183 UDP server");
  int nmea0183_udp_port = port_config_nmea0183_udp_tx->get_port();
  nmea0183_udp_server = new StreamingUDPServer(nmea0183_udp_port, networking);
  nmea0183_udp_server->set_enabled(port_config_nmea0183_udp_tx->get_enabled());

  // send the generated NMEA 0183 message
  if (checkbox_config_translate_to_nmea0183->get_value()) {
    debugD("Connecting NMEA 0183 to consumers");
    n2k_to_0183_transform->connect_to(nmea0183_tcp_server);
    n2k_to_0183_transform->connect_to(nmea0183_udp_server);
  }

  // send the generated SeaSmart message
  if (checkbox_config_translate_to_seasmart->get_value()) {
    debugD("Connecting Seasmart to consumers");
    n2k_to_seasmart_transform->connect_to(nmea0183_tcp_server);
    n2k_to_seasmart_transform->connect_to(nmea0183_udp_server);
  }

  // connect the CAN frame input to the YDWG raw transform
  debugD("Connecting CAN input to YDWG raw transform");
  can_frame_input.connect_to(can_to_ydwg_transform);

  if (port_config_ydwg_raw_tcp_tx->get_enabled()) {
    can_to_ydwg_transform->connect_to(ydwg_raw_tcp_server);
  }

  if (port_config_ydwg_raw_udp->get_tx_enabled()) {
    debugD("Connecting YDWG raw to consumers");
    SetupYellowLEDBlinker(can_to_ydwg_transform);

    can_to_ydwg_transform->connect_to(ydwg_raw_udp_server);
  }
  // ydwg_raw_udp_server->connect_to(new LambdaConsumer<String>(
  //   [](const String &str) { debugD("Received over UDP: %s", str.c_str());
  //   }));

  if (port_config_ydwg_raw_udp->get_rx_enabled()) {
    debugD("Connecting YDWG RAW RX to CAN TX");
    ydwg_raw_udp_server->connect_to(ydwg_raw_to_can_transform)
        ->connect_to(new LambdaConsumer<CANFrame>([](CANFrame frame) {
          // debugD("Sending CAN Frame with ID %d and length %d", frame.id,
          // frame.len);
          can_frame_tx_counter++;
          nmea2000->CANSendFrame(frame.id, frame.len, frame.buf);
        }));
  }
}

String MacAddrToString(uint8_t *mac, bool add_colons) {
  String mac_string = "";
  for (int i = 0; i < 6; i++) {
    char buf[3];
    sprintf(buf, "%02x", mac[i]);
    mac_string += buf;
    if (add_colons) {
      if (i < 5) {
        mac_string += ":";
      }
    }
  }
  return mac_string;
}

void SetupUIComponents() {
  checkbox_config_enable_firmware_updates = new CheckboxConfig(
      true, "Enable", "/System/Enable Firmware Updates",
      "If enabled, the device will periodically check online and "
      "install any available firmware updates.",
      1100);

  port_config_ydwg_raw_tcp_tx = new PortConfig(
      true, kDefaultYdwgRawTCPServerPort, "/Network/YDWG RAW TCP Server",
      "Enable TCP server for transmitting YDWG RAW data.", 1300);

  port_config_ydwg_raw_udp = new BiDiPortConfig(
      true, false, "Transmit to WiFi", "Receive from WiFi",
      kDefaultYdwgRawUDPServerPort, "/Network/YDWG RAW over UDP",
      "Broadcast and/or receive NMEA 2000 traffic as YDWG RAW over UDP.", 1400);

  checkbox_config_translate_to_seasmart = new CheckboxConfig(
      false, "Enable", "/Network/Translate to SeaSmart",
      "Translate NMEA 2000 messages to SeaSmart.Net format. "
      "NMEA 0183 output must be enabled for the SeaSmart.Net messages to "
      "be transmitted.",
      1600);

  checkbox_config_translate_to_nmea0183 = new CheckboxConfig(
      true, "Enable", "/Network/Translate to NMEA 0183",
      "Translate NMEA 2000 messages to NMEA 0183 sentences. "
      "NMEA 0183 output must be enabled for the sentences to "
      "be transmitted.",
      1700);

  port_config_nmea0183_tcp_tx = new PortConfig(
      true, kDefaultNMEA0183TCPServerPort, "/Network/NMEA 0183 TCP Server",
      "Enable a TCP server for transmitting NMEA 0183 and SeaSmart.Net data.",
      1800);

  port_config_nmea0183_udp_tx = new PortConfig(
      true, kDefaultNMEA0183UDPServerPort, "/Network/NMEA 0183 over UDP",
      "Broadcast NMEA 0183 and SeaSmart.Net data over UDP.", 1900);
}

// The setup function performs one-time application initialization.
void setup() {
#ifndef SERIAL_DEBUG_DISABLED
  SetupSerialDebug(115200);
#endif

  // check if we should enter the factory test mode
  if (FactoryTestRequested()) {
    SetupFactoryTest();
    // don't continue with the generic setup
    return;
  }

  // Create a unique hostname for the device.

  uint8_t mac[6];
  WiFi.macAddress(mac);
  String mac_str = MacAddrToString(mac);

  String hostname = "sh-wg";

  SensESPMinimalAppBuilder builder;
  sensesp_app = builder.set_hostname(hostname)->get_app();

  // UI components can only be instantiated once the SensESPBaseApp
  // has been created and filesystem mounted

  SetupUIComponents();

  networking = new Networking("/System/WiFi Settings", "", "",
                              SensESPBaseApp::get_hostname(), "thisisfine");

  networking->set_wifi_manager_ap_ssid(String("Configure SH-wg ") + mac_str);

  // create the MDNS discovery object
  auto mdns_discovery_ = new MDNSDiscovery();

  auto *http_server = new HTTPServer();

  if (checkbox_config_enable_firmware_updates->get_value()) {
    xTaskCreate(ExecuteOTAUpdateTask, "OTAUpdateTask", 8000, NULL, 1, NULL);
  } else {
    debugI("Firmware updates disabled.");
  }

  debugD("Setting up LEDs");

  // enable CAN RX/TX LEDs
  pinMode(kCanLedEnPin, OUTPUT);
  digitalWrite(kCanLedEnPin, HIGH);

  // enable all other LEDs
  pinMode(kRedLedPin, OUTPUT);
  pinMode(kBlueLedPin, OUTPUT);
  pinMode(kYellowLedPin, OUTPUT);
  digitalWrite(kRedLedPin, HIGH);
  digitalWrite(kBlueLedPin, LOW);
  digitalWrite(kYellowLedPin, LOW);

  debugD("Set up the button interface");

  SetupButton();

  debugD("Set up the blue LED blinker");

  SetupBlueLEDBlinker();

  // Initialize the NMEA2000 library
  nmea2000 = new tNMEA2000_esp32_FH(kCanTxPin, kCanRxPin);

  debugD("Initializing NMEA2000...");

  InitNMEA2000();

  // set the system time whenever PGN 126992 is received
  n2k_msg_input.connect_to(new LambdaConsumer<tN2kMsg>(
      [](const tN2kMsg &n2k_msg) { SetSystemTime(n2k_msg); }));

  SetupTransmitters();

  app.onRepeat(1000, []() {
    debugD("Uptime: %lu, CAN RX: %d CAN TX: %d", millis() / 1000,
           can_frame_rx_counter, can_frame_tx_counter);
  });

  // Handle incoming NMEA 2000 messages
  app.onRepeatMicros(50, []() { nmea2000->ParseMessages(); });

  // app.onAvailable(Serial, []() {
  //   // Flush the incoming serial buffer
  //   while (Serial.available()) {
  //     Serial.read();
  //   }
  // });

  sensesp_app->start();
}

void loop() { app.tick(); }
