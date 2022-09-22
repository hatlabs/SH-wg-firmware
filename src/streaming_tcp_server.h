#ifndef SH_WG_FIRMWARE_STREAMING_TCP_SERVER_H_
#define SH_WG_FIRMWARE_STREAMING_TCP_SERVER_H_

#include <Arduino.h>
#include <WiFi.h>

#include <list>
#include <memory>

#include "origin_string.h"
#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/valueconsumer.h"
#include "shwg.h"

using namespace sensesp;

constexpr size_t kMaxClients = 10;
constexpr size_t kRXBufferSize = 512;

using WiFiClientPtr = std::shared_ptr<WiFiClient>;

/**
 * @brief TCP client connection container with RX buffer.
 */
class BufferedTCPClient {
 public:
  BufferedTCPClient(WiFiClientPtr client) : client_{client} {}

  WiFiClientPtr client_;

  int available() { return client_->available(); }

  int read_line(String& line) {
    while (client_->available()) {
      char c = client_->read();
      rx_buf_[rx_pos_++] = c;
      if (rx_pos_ == kRXBufferSize-1) {
        debugW("RX buffer overflow");
        rx_pos_ = 0;
      } else if (c == '\n') {
        // received a full line
        rx_buf_[rx_pos_] = '\0';
        int received = rx_pos_;
        rx_pos_ = 0;
        line = rx_buf_;
        return received;
      }
    }
    return 0;
  }

 protected:
  char rx_buf_[kRXBufferSize];
  int rx_pos_ = 0;
};

/**
 * @brief TCP server that is able to receive and transmit continuous data
 * streams.
 *
 */
class StreamingTCPServer : public ValueProducer<OriginString>,
                           public ValueConsumer<OriginString>,
                           public Startable {
 public:
  StreamingTCPServer(const uint16_t port, Networking *networking)
      : Startable(50), networking_{networking}, port_{port} {
    server_ = new WiFiServer(port);

    ReactESP::app->onRepeatMicros(100, [this]() {
      this->check_connections();
      this->check_client_input();
    });
  }

  void send_buf(OriginString value) {
    // debugD("Sending: %s", buf);
    for (auto it = clients_.begin(); it != clients_.end(); it++) {
      if ((*it).client_ != NULL && (*it).client_->connected() &&
          value.origin_id != origin_id(&((*it).client_))) {
        (*it).client_->write(value.data.c_str());
      }
    }
  }

  void set_input(OriginString new_value, uint8_t input_channel = 0) override {
    send_buf(new_value);
  }

  void set_enabled(bool enabled) { enabled_ = enabled; }

 protected:
  Networking *networking_;
  WiFiServer *server_;
  const uint16_t port_;

  bool enabled_ = true;

  std::list<BufferedTCPClient> clients_;

  void add_client(WiFiClient &client) {
    debugD("New client connected");
    clients_.push_back(BufferedTCPClient(WiFiClientPtr(new WiFiClient(client))));
  }

  void stop_client(std::list<BufferedTCPClient>::iterator &it) {
    debugD("Client disconnected");
    (*it).client_->stop();
    it = clients_.erase(it);
  }

  void check_connections() {
    // listen for incoming clients
    WiFiClient client = server_->available();

    if (client) {
      add_client(client);
    }

    for (auto it = clients_.begin(); it != clients_.end(); it++) {
      if ((*it).client_ != NULL) {
        if (!(*it).client_->connected()) {
          stop_client(it);
        }
      } else {
        debugW("Client did not get automatically erased");
        it = clients_.erase(it);  // Should have been erased by StopClient
      }
    }
  }

  void check_client_input() {
    for (auto it = clients_.begin(); it != clients_.end(); it++) {
      if ((*it).client_ != NULL && (*it).client_->connected()) {
        String line;
        while ((*it).read_line(line)) {
          OriginString value{origin_id(&((*it).client_)), line};
          this->emit(value);
        }
      }
    }
  }

  void start() override {
    if (enabled_) {
      networking_->connect_to(
          new LambdaConsumer<WifiState>([this](WifiState state) {
            if ((state == WiFiState::kWifiConnectedToAP) ||
                (state == WiFiState::kWifiAPModeActivated)) {
              debugI("Starting Streaming TCP server on port %d", port_);
              server_->begin();
            }
          }));
    }
  }
};

#endif  // SH_WG_FIRMWARE_STREAMING_TCP_SERVER_H_
