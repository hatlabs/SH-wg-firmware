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
#include "elapsedMillis.h"
#include "n2k_nmea0183_transform.h"
#include "sensesp/net/http_server.h"
#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/transforms/lambda_transform.h"
#include "sensesp_minimal_app_builder.h"
#include "streaming_tcp_server.h"
#include "streaming_udp_server.h"
#include "time_string.h"
#include "ydwg_raw.h"

using namespace sensesp;

constexpr gpio_num_t kCanRxPin = GPIO_NUM_34;
constexpr gpio_num_t kCanTxPin = GPIO_NUM_32;

#define MAX_NMEA2000_MESSAGE_SEASMART_SIZE 500
#define MAX_NMEA0183_MESSAGE_SIZE 200

constexpr uint16_t kSeasmartTCPServerPort = 2222;
constexpr uint16_t kYdwgRawTCPServerPort = 2223;

constexpr uint16_t kSeasmartUDPServerPort = 2000;

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
    0};

tNMEA2000_esp32_FH *nmea2000;

StreamingTCPServer *seasmart_tcp_server;
StreamingTCPServer *ydwg_raw_tcp_server;

StreamingUDPServer *seasmart_udp_server;

// update the system time every hour
constexpr unsigned long kTimeUpdatePeriodMs = 3600 * 1000;
// time elapsed since last system time update
elapsedMillis elapsed_since_last_system_time_update = kTimeUpdatePeriodMs;

reactesp::ReactESP app;

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
  nmea2000 = new tNMEA2000_esp32_FH(kCanTxPin, kCanRxPin);

  debugD("Initializing NMEA2000...");

  InitNMEA2000();

  auto ydwg_raw_transform =
      new LambdaTransform<CANFrame, String>([](CANFrame frame) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        String raw_string = CANFrameToYDWGRaw(frame, tv);
        return raw_string;
      });

  // set the system time whenever PGN 126992 is received
  n2k_msg_input.connect_to(new LambdaConsumer<tN2kMsg>(
      [](const tN2kMsg &n2k_msg) { SetSystemTime(n2k_msg); }));

  auto n2k_to_0183_transform = new N2KTo0183Transform();
  auto n2k_to_seasmart_transform = new SeasmartTransform();
  // the message handler called within this consumer will write its output
  // to nmea0183_msg_observable
  n2k_msg_input.connect_to(n2k_to_0183_transform);
  n2k_msg_input.connect_to(n2k_to_seasmart_transform);

  auto *networking = new Networking(
      "/system/net", "", "", SensESPBaseApp::get_hostname(), "thisisfine");
  auto *http_server = new HTTPServer();

  seasmart_tcp_server =
      new StreamingTCPServer(kSeasmartTCPServerPort, networking);
  ydwg_raw_tcp_server =
      new StreamingTCPServer(kYdwgRawTCPServerPort, networking);

  seasmart_udp_server =
      new StreamingUDPServer(kSeasmartUDPServerPort, networking);

  // send the generated NMEA 0183 message
  n2k_to_0183_transform->connect_to(seasmart_tcp_server);
  n2k_to_0183_transform->connect_to(seasmart_udp_server);

  // send the generated SeaSmart message
  n2k_to_seasmart_transform->connect_to(seasmart_tcp_server);
  n2k_to_seasmart_transform->connect_to(seasmart_udp_server);

  // connect the CAN frame input to the YDWG raw transform
  can_frame_input.connect_to(ydwg_raw_transform)
      ->connect_to(ydwg_raw_tcp_server);

  // Handle incoming NMEA 2000 messages
  app.onRepeat(1, []() { nmea2000->ParseMessages(); });

  // app.onAvailable(Serial, []() {
  //   // Flush the incoming serial buffer
  //   while (Serial.available()) {
  //     Serial.read();
  //   }
  // });

  sensesp_app->start();
}

void loop() { app.tick(); }
