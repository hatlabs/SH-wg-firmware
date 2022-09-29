#ifndef SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
#define SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_

#include <Arduino.h>
#include <AsyncUDP.h>
#include <WiFi.h>

#include "sensesp/net/networking.h"
#include "sensesp/system/task_queue_producer.h"
#include "sensesp/system/valueconsumer.h"
#include "origin_string.h"

using namespace sensesp;

class StreamingUDPServer : public ValueProducer<OriginString>,
                           public ValueConsumer<OriginString>,
                           public Startable {
 public:
  StreamingUDPServer(const uint16_t port, Networking* networking)
      : Startable(50), networking_{networking}, port_{port} {
    task_queue_producer_ = new TaskQueueProducer<OriginString*>(NULL, ReactESP::app, 200, 490);
  }

  void set_input(OriginString new_value, uint8_t input_channel = 0) override {
    if (connected_ && new_value.origin_id != origin_id(&async_udp_)) {
      size_t len_sent = async_udp_.broadcast(new_value.data.c_str());
      if (len_sent == 0) {
        debugW("UDP broadcast failed: %s", new_value.data.c_str());
      }
    }
  }

  void set_enabled(bool enabled) { enabled_ = enabled; }

 protected:
  Networking* networking_;
  const uint16_t port_;
  AsyncUDP async_udp_;
  bool connected_ = false;
  TaskQueueProducer<OriginString*>* task_queue_producer_;

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

                  OriginString* ydwg_string = new OriginString{
                      origin_id(&async_udp_), buf};
                  //  Handle the received packet in the main task
                  //  Note that there is a minor possibility of a memory leak
                  //  if the task is not handled in time.
                  task_queue_producer_->set(ydwg_string);
                });
              } else {
                debugE("UDP Server startup failed - port reserved?");
              }
            }
          }));
      task_queue_producer_->connect_to(
          new LambdaConsumer<OriginString*>([this](OriginString* ydwg_str) {
            this->emit(*ydwg_str);
            delete ydwg_str;
          }));
    }
  }
};

#endif  // SH_WG_FIRMWARE_STREAMING_UDP_SERVER_H_
