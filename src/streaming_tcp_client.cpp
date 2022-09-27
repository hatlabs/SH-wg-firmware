#include "streaming_tcp_client.h"

using namespace sensesp;

void ExecuteTCPClientTask(void* this_ptr) {
  // cast this_ptr into a pointer to a StreamingTCPClient
  StreamingTCPClient* this_ = (StreamingTCPClient*)this_ptr;

  this_->execute_client_task();
}

void StreamingTCPClient::start() {
  if (enabled_) {
    xTaskCreate(ExecuteTCPClientTask, "tcp_client_task", 4096, this, 1, NULL);

    // emit received OriginStrings in the main task
    rx_queue_producer_->connect_to(
        new LambdaConsumer<OriginString*>([this](OriginString* origin_str) {
          this->emit(*origin_str);
          delete origin_str;
        }));
  }
}
