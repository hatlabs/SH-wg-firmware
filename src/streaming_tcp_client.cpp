#include "streaming_tcp_client.h"

using namespace sensesp;

void ExecuteTCPClientTask(void* this_ptr) {
  // cast this_ptr into a pointer to a StreamingTCPClient
  StreamingTCPClient* this_ = (StreamingTCPClient*)this_ptr;

  this_->execute_client_task();
}