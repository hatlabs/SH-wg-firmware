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
#include "can_frame.h"
#include "concatenate_strings.h"
#include "config.h"
#include "filter_transform.h"
#include "firmware_info.h"
#include "n2k_nmea0183_transform.h"
#include "origin_string.h"
#include "ota_update_task.h"
#include "seasmart_transform.h"
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
#include "streaming_tcp_client.h"
#include "streaming_tcp_server.h"
#include "streaming_udp_server.h"
#include "stringtokenizer_transform.h"
#include "time_string.h"
#include "ui_controls.h"
#include "ydwg_raw_output.h"
#include "ydwg_raw_parser.h"

using namespace sensesp;

// Set the information for other bus devices, which messages we support
const unsigned long kTransmitMessages[] = {0};
const unsigned long ReceiveMessages[] = {
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

StreamingTCPClient *ydwg_raw_tcp_client;
StreamingTCPClient *nmea0183_tcp_client;

// time elapsed since last system time update
elapsedMillis elapsed_since_last_system_time_update = kTimeUpdatePeriodMs;

reactesp::ReactESP app;

SensESPMinimalApp *sensesp_app;

Networking *networking;

CheckboxConfig *checkbox_config_enable_firmware_updates;
BiDiPortConfig *port_config_ydwg_raw_tcp;
HostPortConfig *port_config_ydwg_raw_tcp_client;
BiDiPortConfig *port_config_ydwg_raw_udp;
CheckboxConfig *checkbox_config_translate_to_seasmart;
CheckboxConfig *checkbox_config_translate_to_nmea0183;
PortConfig *port_config_nmea0183_tcp_tx;
HostPortConfig *port_config_nmea0183_tcp_client;
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
UILambdaOutput<String> ui_output_wifi_ssid =
    UILambdaOutput<String>("SSID", []() { return WiFi.SSID(); }, "WiFi", 230);
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

UILambdaOutput<int> ui_output_free_heap(
    "Free memory", []() { return ESP.getFreeHeap(); }, "Runtime", 410);

int led_state = -1;

uint64_t GetBoardSerialNumber() {
  uint8_t chipid[6];
  esp_efuse_mac_get_default(chipid);
  return ((uint64_t)chipid[0] << 0) + ((uint64_t)chipid[1] << 8) +
         ((uint64_t)chipid[2] << 16) + ((uint64_t)chipid[3] << 24) +
         ((uint64_t)chipid[4] << 32) + ((uint64_t)chipid[5] << 40);
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

ObservableValue<tN2kMsg> n2k_msg_input;
ObservableValue<CANFrame> can_frame_input;

// All CAN frame producers and consumers connect to the clearinghouse
LambdaTransform<CANFrame, CANFrame> *can_frame_clearinghouse;

// Consumer that will send CAN frames to the NMEA 2000 bus
LambdaConsumer<CANFrame> *can_frame_sender;

void InitNMEA2000() {
  nmea2000->SetN2kCANMsgBufSize(8);
  nmea2000->SetN2kCANReceiveFrameBufSize(100);
  //  nmea2000->SetForwardStream(&Serial);  // PC output on due native port
  //  nmea2000->SetForwardType(tNMEA2000::fwdt_Text); // Show in clear text
  // nmea2000->EnableForward(false);                 // Disable all msg
  // forwarding to USB (=Serial)

  char serial_number_str[33];  // Model serial code in N2K is 32 characters
  uint64_t serial_number = GetBoardSerialNumber();
  snprintf(serial_number_str, 32, "%" PRIu64, (long unsigned int)serial_number);

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
      frame.origin_type = CANFrameOriginType::kLocal;
      frame.origin_id = origin_id(nmea2000);
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
            // blink the blue LED at 2 Hz and 12.5% duty cycle
            ledcWrite(kBluePWMChannel, 65536 / 8);
            break;
          case WiFiState::kWifiAPModeActivated:
            ledcAttachPin(kBlueLedPin, kBluePWMChannel);
            // blink the blue LED at 2 Hz and 87.5% duty cycle
            ledcWrite(kBluePWMChannel, 65536 * 7 / 8);
            break;
          default:
            digitalWrite(kBlueLedPin, LOW);
            break;
        }
      });

  networking->connect_to(wifi_state_consumer);
}

static void SetupYellowLEDBlinker(
    ValueProducer<OriginString> *string_producer) {
  static int solid_on_pattern[] = {1000, 0, PATTERN_END};
  auto blinker = new PatternBlinker(kYellowLedPin, solid_on_pattern);

  string_producer->connect_to(
      new LambdaConsumer<OriginString>([blinker](const OriginString &str) {
        if (WiFi.isConnected()) {
          blinker->blip(5);
        }
      }));
}

static void SetupConnections() {
  can_frame_clearinghouse = new LambdaTransform<CANFrame, CANFrame>(
      [](const CANFrame &frame) { return frame; });

  auto can_to_ydwg_transform =
      new LambdaTransform<CANFrame, OriginString>([](CANFrame frame) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        OriginString origin_string = CANFrameToYDWGRaw(frame, tv);
        return origin_string;
      });

  can_frame_sender = new LambdaConsumer<CANFrame>([](CANFrame frame) {
    // debugD("Sending CAN Frame with ID %d and length %d", frame.id,
    // frame.len);

    if (frame.origin_id == origin_id(nmea2000)) {
      // ignore frames that we just received
      return;
    }

    if (frame.origin_type == CANFrameOriginType::kRemoteApp) {
      // Ignore YDWG RAW messages with 'T' direction
      return;
    }
    can_frame_tx_counter++;
    if (frame.origin_type == CANFrameOriginType::kApp) {
      // Application format messages need to have their source address
      // replaced with our own source address.

      unsigned char our_source = nmea2000->GetN2kSource(0);
      uint32_t frame_id = frame.id;
      // clear existing source address
      frame_id &= ~0xFF;
      // set new source address
      frame_id |= our_source;

      frame.id = frame_id;
    }
    nmea2000->CANSendFrame(frame.id, frame.len, frame.buf);
  });

  auto concatenate_ydwg_strings = new ConcatenateStrings(100, 1000);
  auto concatenate_n0183_strings = new ConcatenateStrings(100, 1000);

  auto string_tokenizer = new StringTokenizer("\r\n");

  auto n2k_to_0183_transform = new N2KTo0183Transform(nmea2000);
  auto n2k_to_seasmart_transform = new SeasmartTransform(nmea2000);
  auto ydwg_raw_to_can_transform = new YDWGRawToCANFrameTransform();

  can_to_ydwg_transform->connect_to(concatenate_ydwg_strings);
  string_tokenizer->connect_to(ydwg_raw_to_can_transform);

  //////
  // N2K message routing

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

  //////
  // CAN frame routing

  can_frame_input.connect_to(can_frame_clearinghouse);
  can_frame_clearinghouse->connect_to(can_frame_sender);
  ydwg_raw_to_can_transform->connect_to(can_frame_clearinghouse);

  // set up the YDWG RAW TCP server

  debugD("Setting up YDWG RAW TCP server");
  int ydwg_raw_tcp_port = port_config_ydwg_raw_tcp->get_port();
  ydwg_raw_tcp_server = new StreamingTCPServer(ydwg_raw_tcp_port, networking);
  if (!port_config_ydwg_raw_tcp->get_tx_enabled() &&
      !port_config_ydwg_raw_tcp->get_rx_enabled()) {
    ydwg_raw_tcp_server->set_enabled(false);
  }

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
    n2k_to_0183_transform->connect_to(concatenate_n0183_strings);
    concatenate_n0183_strings->connect_to(nmea0183_udp_server);
  }

  // send the generated SeaSmart message
  if (checkbox_config_translate_to_seasmart->get_value()) {
    debugD("Connecting Seasmart to consumers");
    n2k_to_seasmart_transform->connect_to(nmea0183_tcp_server);
    n2k_to_seasmart_transform->connect_to(concatenate_n0183_strings);
  }

  // set up a YDWG RAW TCP client
  if (port_config_ydwg_raw_tcp_client->get_enabled()) {
    String ydwg_raw_tcp_client_host =
        port_config_ydwg_raw_tcp_client->get_host();
    int ydwg_raw_tcp_client_port = port_config_ydwg_raw_tcp_client->get_port();
    ydwg_raw_tcp_client = new StreamingTCPClient(
        ydwg_raw_tcp_client_host, ydwg_raw_tcp_client_port, networking);
    can_to_ydwg_transform->connect_to(ydwg_raw_tcp_client);
    ydwg_raw_tcp_client->connect_to(string_tokenizer);
  }

  // set up an NMEA 0183 TCP client
  if (port_config_nmea0183_tcp_client->get_enabled()) {
    String nmea0183_tcp_client_host =
        port_config_nmea0183_tcp_client->get_host();
    int nmea0183_tcp_client_port = port_config_nmea0183_tcp_client->get_port();
    nmea0183_tcp_client = new StreamingTCPClient(
        nmea0183_tcp_client_host, nmea0183_tcp_client_port, networking);
    n2k_to_0183_transform->connect_to(nmea0183_tcp_client);
  }

  // connect the CAN frame input to the YDWG raw transform
  debugD("Connecting CAN input to YDWG raw transform");
  can_frame_clearinghouse->connect_to(can_to_ydwg_transform);

  if (port_config_ydwg_raw_tcp->get_tx_enabled()) {
    debugD("Connecting YDWG RAW TX to TCP server");
    can_to_ydwg_transform->connect_to(ydwg_raw_tcp_server);
  }

  if (port_config_ydwg_raw_tcp->get_rx_enabled()) {
    debugD("Connecting TCP server to YDWG RAW RX");
    ydwg_raw_tcp_server->connect_to(ydwg_raw_to_can_transform);
  }

  if (port_config_ydwg_raw_udp->get_tx_enabled()) {
    debugD("Connecting YDWG RAW to UDP TX");
    SetupYellowLEDBlinker(can_to_ydwg_transform);

    concatenate_ydwg_strings->connect_to(ydwg_raw_udp_server);
  }

  if (port_config_ydwg_raw_udp->get_rx_enabled()) {
    debugD("Connecting UDP RX to YDWG RAW");
    ydwg_raw_udp_server->connect_to(string_tokenizer);
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

void PrintProductInfo() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  String mac_str = MacAddrToString(mac);

  Serial.println("***** Product Info *****");
  Serial.printf("%s %s %s\n", kFirmwareName, kFirmwareVersion, mac_str.c_str());
  Serial.println("************************");
}

void SetupUIComponents() {
  checkbox_config_enable_firmware_updates = new CheckboxConfig(
      true, "Enable", "/System/Enable Firmware Updates",
      "If enabled, the device will periodically check online and "
      "install any available firmware updates.",
      1100);

  port_config_ydwg_raw_tcp = new BiDiPortConfig(
      true, false, "Transmit to WiFi", "Receive from WiFi",
      kDefaultYdwgRawTCPServerPort, "/Network/YDWG RAW TCP Server",
      "Enable TCP server for transmitting and/or receiving YDWG RAW data.",
      1300);

  port_config_ydwg_raw_tcp_client = new HostPortConfig(
      false, "", kDefaultYdwgRawTCPServerPort, "Enabled", "Server hostname",
      "Server port", "/Network/YDWG RAW TCP Client",
      "Connect to another TCP server for transmitting and receiving YDWG RAW "
      "data.",
      1350);

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

  port_config_nmea0183_tcp_client = new HostPortConfig(
      false, "", kDefaultNMEA0183TCPServerPort, "Enabled", "Server hostname",
      "Server port", "/Network/NMEA 0183 TCP Client",
      "Connect to another TCP server for transmitting NMEA 0183 and "
      "SeaSmart.Net data.",
      1850);

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

  // Print product information on the serial port
  PrintProductInfo();

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
                              SensESPBaseApp::get_hostname(),
                              kWiFiCaptivePortalPassword);

  networking->set_wifi_manager_ap_ssid(String("Configure SH-wg ") + mac_str);

  networking->connect_to(new LambdaConsumer<WiFiState>([](WiFiState state) {
    // turn of WiFi power saving when connected
    if (state == WiFiState::kWifiConnectedToAP) {
      debugD("Disabling WiFi power saving");
      WiFi.setSleep(false);
    }
  }));

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

  SetupConnections();

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
