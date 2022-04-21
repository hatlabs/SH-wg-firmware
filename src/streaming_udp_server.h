#ifndef SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
#define SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_

#include <Arduino.h>
#include <AsyncUDP.h>
#include <WiFi.h>

#include "sensesp/net/networking.h"
#include "sensesp/system/valueconsumer.h"

using namespace sensesp;

class StreamingUDPServer : public ValueProducer<String>, public ValueConsumer<String>, public Startable {
 public:
  StreamingUDPServer(const uint16_t port, Networking *networking)
      : Startable(50), networking_{networking}, port_{port} {}

  void set_input(String new_value, uint8_t input_channel = 0) override {
    if (connected_) {
      String str_nl = new_value;
      async_udp_.broadcast(str_nl.c_str());
    }
  }

 protected:
  Networking *networking_;
  const uint16_t port_;
  AsyncUDP async_udp_;
  bool connected_ = false;

  void start() override {
    networking_->connect_to(
        new LambdaConsumer<WifiState>([this](WifiState state) {
          if (state == WifiState::kWifiConnectedToAP) {
            debugI("Starting Streaming UDP server on port %d", port_);
            if (async_udp_.listen(port_)) {
              connected_ = true;
              async_udp_.onPacket([this](AsyncUDPPacket packet) {
                // ensure that the received packet is zero-terminated
                char buf[packet.length() + 1];
                memcpy(buf, packet.data(), packet.length());
                buf[packet.length()] = '\0';

                String packet_str = String(buf);
                this->emit(packet_str);
              });
            } else {
              debugE("UDP Server startup failed - port reserved?");
            }
          }
        }));
  }
};

#endif  // SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
