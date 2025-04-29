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

void ExecuteTCPClientTask(void* this_ptr);

/**
 * @brief TCP client that is able to receive and transmit continuous data
 * streams.
 */
class StreamingTCPClient : public ValueProducer<OriginString>,
                           public ValueConsumer<OriginString> {
 public:
  StreamingTCPClient(const String& host, const uint16_t port,
                     const std::shared_ptr<Networking> networking)
      : networking_{networking}, host_{host}, port_{port} {
    client_ = new BufferedTCPClient(WiFiClientPtr(new WiFiClient()));
    tx_queue_producer_ =
        new TaskQueueProducer<OriginString*>(NULL, 200);
    rx_queue_producer_ =
        new TaskQueueProducer<OriginString*>(NULL, 200);

    event_loop()->onDelay(0, [this]() {
      if (enabled_) {
        xTaskCreate(ExecuteTCPClientTask, "tcp_client_task", 4096, this, 1, NULL);

        // emit received OriginStrings in the main task
        rx_queue_producer_->connect_to(
            new LambdaConsumer<OriginString*>([this](OriginString* origin_str) {
              this->emit(*origin_str);
              delete origin_str;
            }));
      } });
  }

  void set(const OriginString &new_value) override {
    OriginString* value_ptr = new OriginString(new_value);
    tx_queue_producer_->set(value_ptr);
  }

  void set_enabled(bool enabled) { enabled_ = enabled; }

 protected:
  const std::shared_ptr<Networking> networking_;
  const String host_;
  const uint16_t port_;

  BufferedTCPClient* client_;

  TaskQueueProducer<OriginString*>* tx_queue_producer_;
  TaskQueueProducer<OriginString*>* rx_queue_producer_;

  ObservableValue<OriginString> tx_string_;

  bool enabled_ = true;

  void execute_client_task() {
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

    event_loop()->onRepeat(2000, [this]() {
      if (client_->client_->connected()) {
        // Send an empty line as a keepalive message. Without this,
        // disconnection detection takes just about forever.
        client_->client_->write("\r\n");
      }
    });

    event_loop()->onRepeat(100, [this]() {
      // flush the TCP client TX buffer
      if (client_->client_->connected()) {
        client_->client_->clear();
      }
    });

    tx_string_.connect_to(send_data);

    // receive any data sent to the client
    event_loop()->onRepeat(1, [this]() {
      if (client_->available() || client_->client_->connected()) {
        String line;
        int retval;
        while (this->client_->read_line(line)) {
          OriginString* value =
              new OriginString{origin_id(&client_->client_), line};
          this->rx_queue_producer_->set(value);
          if (retval == false) {
            debugW(
                "StreamingTCPClient: rx_queue_producer_ full, dropping value");
            delete value;
          }
        }
      }
    });

    // try to establish a connection to the server
    event_loop()->onRepeat(1000, [this]() {
      if (!client_->client_->connected()) {
        client_->client_->stop();
        client_->clear_buf();
        debugD("Connecting to %s:%d...", host_.c_str(), port_);
        client_->client_->connect(host_.c_str(), port_);
        debugD("Connected");
      }
    });

    while (true) {
      event_loop()->tick();

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
