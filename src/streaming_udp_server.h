#ifndef SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
#define SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>

#include "sensesp/net/networking.h"
#include "sensesp/system/valueconsumer.h"

using namespace sensesp;

class StreamingUDPServer : public ValueConsumer<String>, public Startable {
 public:
  StreamingUDPServer(const uint16_t port, Networking *networking)
      : Startable(50), networking_{networking}, port_{port} {

  }

  void set_input(String new_value, uint8_t input_channel = 0) override {
    String str_nl = new_value;
    async_udp_.broadcast(str_nl.c_str());
  }
 protected:
  Networking *networking_;
  const uint16_t port_;
  AsyncUDP async_udp_;

  void start() override {
    debugI("Starting Streaming UDP server on port %d", port_);
    if (async_udp_.listen(port_)) {
      debugD("Startup successful");
    } else {
      debugE("Startup failed - port reserved?");
    }
  }
};

#endif // SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
