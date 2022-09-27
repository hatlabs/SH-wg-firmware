#ifndef SH_WG_FIRMWARE_STREAMING_TCP_CLIENT_H_
#define SH_WG_FIRMWARE_STREAMING_TCP_CLIENT_H_

#include <Arduino.h>
#include <WiFi.h>

#include "buffered_tcp_client.h"
#include "origin_string.h"
#include "sensesp/net/networking.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/task_queue_producer.h"
#include "shwg.h"

using namespace sensesp;

/**
 * @brief TCP client that is able to receive and transmit continuous data
 * streams.
 */
class StreamingTCPClient : public ValueProducer<OriginString>,
                           public ValueConsumer<OriginString>,
                           public Startable {
 public:
  StreamingTCPClient(const String& host, const uint16_t port,
                     Networking* networking)
      : Startable(50), networking_{networking}, host_{host}, port_{port} {
    client_ = new BufferedTCPClient(WiFiClientPtr(new WiFiClient()));
    tx_queue_producer_ = new TaskQueueProducer<OriginString*>(NULL, 20, 491);
    rx_queue_producer_ = new TaskQueueProducer<OriginString*>(NULL, 20, 492);
  }

  void set_input(OriginString new_value, uint8_t input_channel = 0) override {
    OriginString *value_ptr = new OriginString(new_value);
    tx_queue_producer_->set(value_ptr);
  }

  void set_enabled(bool enabled) { enabled_ = enabled; }

 protected:
  Networking* networking_;
  const String host_;
  const uint16_t port_;

  BufferedTCPClient* client_;

  TaskQueueProducer<OriginString*>* tx_queue_producer_;
  TaskQueueProducer<OriginString*>* rx_queue_producer_;

  ObservableValue<OriginString> tx_string_;

  ReactESP* task_app_ = nullptr;

  bool enabled_ = true;

  void start() override;

  void execute_client_task() {
    task_app_ = new ReactESP(false);
    // Receive strings to be transmitted in the tcp client task.
    // We don't want consumers to connect to the task queue directly, because
    // we're responsible for deleting the received string objects.
    this->tx_queue_producer_->connect_to(
        new LambdaConsumer<OriginString*>([this](OriginString* origin_str) {
          this->tx_string_ = *origin_str;
          delete origin_str;
        }));

    auto send_data =
        new LambdaConsumer<OriginString>([this](OriginString origin_str) {
          if (client_->client_->connected() &&
            origin_str.origin_id != origin_id(&client_->client_)) {
            client_->client_->write(origin_str.data.c_str());
          }
        });

    task_app_->onRepeat(2000, [this]() {
      if (client_->client_->connected()) {
        // Send an empty line as a keepalive message. Without this,
        // disconnection detection takes just about forever.
        client_->client_->write("\r\n");
      }
    });

    task_app_->onRepeat(100, [this]() {
      // flush the TCP client TX buffer
      if (client_->client_->connected()) {
        client_->client_->flush();
      }
    });

    tx_string_.connect_to(send_data);

    // receive any data sent to the client
    task_app_->onRepeat(1, [this]() {
      if (client_->client_->connected()) {
        String line;
        while (this->client_->read_line(line)) {
          OriginString* value = new OriginString{origin_id(&client_->client_), line};
          this->rx_queue_producer_->set(value);
        }
      }
    });

    // try to establish a connection to the server
    task_app_->onRepeat(1000, [this]() {
      if (!client_->client_->connected()) {
        client_->client_->stop();
        client_->clear_buf();
        debugD("Connecting to %s:%d...", host_.c_str(), port_);
        client_->client_->connect(host_.c_str(), port_);
        debugD("Connected");
      }
    });

    while (true) {
      task_app_->tick();

      // A small delay required to prevent the task watchdog from triggering.
      // This also limits the maximum packet rate but greatly reduces
      // idle CPU load.
      delay(1);
    }
  }

  // a new task entry point is always a plain function; use this friend
  // to route the execution back to this class
  friend void ExecuteTCPClientTask(void* task_args);
};

#endif  // SH_WG_FIRMWARE_STREAMING_TCP_CLIENT_H_
