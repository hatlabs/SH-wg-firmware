#ifndef SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
#define SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_

#include <Arduino.h>
#include <AsyncUDP.h>
#include <WiFi.h>

#include "sensesp/net/networking.h"
#include "sensesp/system/task_queue_producer.h"
#include "sensesp/system/valueconsumer.h"

using namespace sensesp;

class StreamingUDPServer : public ValueProducer<String>,
                           public ValueConsumer<String>,
                           public Startable {
 public:
  StreamingUDPServer(const uint16_t port, Networking* networking)
      : Startable(50), networking_{networking}, port_{port} {
    task_queue_producer_ = new TaskQueueProducer<String*>(NULL, 10, 490);
  }

  void set_input(String new_value, uint8_t input_channel = 0) override {
    if (connected_) {
      String str_nl = new_value;
      async_udp_.broadcast(str_nl.c_str());
    }
  }

  void set_enabled(bool enabled) { enabled_ = enabled; }

 protected:
  Networking* networking_;
  const uint16_t port_;
  AsyncUDP async_udp_;
  bool connected_ = false;
  TaskQueueProducer<String*>* task_queue_producer_;

  bool enabled_ = true;

  void start() override {
    if (enabled_) {
      networking_->connect_to(
          new LambdaConsumer<WifiState>([this](WifiState state) {
            if ((state == WiFiState::kWifiConnectedToAP) ||
                (state == WiFiState::kWifiAPModeActivated)) {
              debugI("Starting Streaming UDP server on port %d", port_);
              if (async_udp_.listen(port_)) {
                connected_ = true;
                async_udp_.onPacket([this](AsyncUDPPacket packet) {
                  // ensure that the received packet is zero-terminated
                  char buf[packet.length() + 1];
                  memcpy(buf, packet.data(), packet.length());
                  buf[packet.length()] = '\0';

                  String* packet_str = new String(buf);
                  // Handle the received packet in the main task
                  // Note that there is a minor possibility of a memory leak
                  // if the task is not handled in time.
                  task_queue_producer_->set(packet_str);
                });
              } else {
                debugE("UDP Server startup failed - port reserved?");
              }
            }
          }));
      task_queue_producer_->connect_to(
          new LambdaConsumer<String*>([this](String* packet_str) {
            this->emit(*packet_str);
            delete packet_str;
          }));
    }
  }
};

#endif  // SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
