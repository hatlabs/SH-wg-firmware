#ifndef SH_WG_FIRMWARE_BUFFERED_TCP_CLIENT_H_
#define SH_WG_FIRMWARE_BUFFERED_TCP_CLIENT_H_

#include <Arduino.h>
#include <WiFi.h>

#include "origin_string.h"
#include "shwg.h"

using namespace sensesp;

using WiFiClientPtr = std::shared_ptr<WiFiClient>;

constexpr size_t kRXBufferSize = 512;

/**
 * @brief TCP client connection container with RX buffer.
 */
class BufferedTCPClient {
 public:
  BufferedTCPClient(WiFiClientPtr client) : client_{client} {}

  WiFiClientPtr client_;

  int available() { return client_->available(); }

  void clear_buf() {
    rx_pos_ = 0;
  }

  int read_line(String& line) {
    while (client_->available()) {
      char c = client_->read();
      rx_buf_[rx_pos_++] = c;
      if (rx_pos_ == kRXBufferSize - 1) {
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

#endif  // SH_WG_FIRMWARE_BUFFERED_TCP_CLIENT_H_
