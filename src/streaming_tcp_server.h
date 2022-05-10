#ifndef SH_WG_FIRMWARE_STREAMING_TCP_SERVER_H_
#define SH_WG_FIRMWARE_STREAMING_TCP_SERVER_H_

#include <Arduino.h>
#include <WiFi.h>

#include <list>
#include <memory>

#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/valueconsumer.h"

using namespace sensesp;

const size_t kMaxClients = 10;

using WiFiClientPtr = std::shared_ptr<WiFiClient>;

class StreamingTCPServer : public ValueConsumer<String>, public Startable {
 public:
  StreamingTCPServer(const uint16_t port, Networking *networking)
      : Startable(50), networking_{networking}, port_{port} {
    server_ = new WiFiServer(port);

    ReactESP::app->onRepeat(1, [this]() { this->check_connections(); });
  }

  void send_buf(const char *buf) {
    // debugD("Sending: %s", buf);
    for (auto it = clients_.begin(); it != clients_.end(); it++) {
      if ((*it) != NULL && (*it)->connected()) {
        (*it)->print(buf);
      }
    }
  }

  void set_input(String new_value, uint8_t input_channel = 0) override {
    send_buf(new_value.c_str());
  }

  void set_enabled(bool enabled) { enabled_ = enabled; }

 protected:
  Networking *networking_;
  WiFiServer *server_;
  const uint16_t port_;

  bool enabled_ = true;

  std::list<WiFiClientPtr> clients_;

  void add_client(WiFiClient &client) {
    debugD("New client connected");
    clients_.push_back(WiFiClientPtr(new WiFiClient(client)));
  }

  void stop_client(std::list<WiFiClientPtr>::iterator &it) {
    debugD("Client disconnected");
    (*it)->stop();
    it = clients_.erase(it);
  }

  void check_connections() {
    // listen for incoming clients
    WiFiClient client = server_->available();

    if (client) {
      add_client(client);
    }

    for (auto it = clients_.begin(); it != clients_.end(); it++) {
      if ((*it) != NULL) {
        if (!(*it)->connected()) {
          stop_client(it);
        }
      } else {
        it = clients_.erase(it);  // Should have been erased by StopClient
      }
    }
  }

  void start() override {
    if (enabled_) {
      networking_->connect_to(
          new LambdaConsumer<WifiState>([this](WifiState state) {
            if (state == WifiState::kWifiConnectedToAP) {
              debugI("Starting Streaming TCP server on port %d", port_);
              server_->begin();
            }
          }));
    }
  }
};

#endif  // SH_WG_FIRMWARE_STREAMING_TCP_SERVER_H_
