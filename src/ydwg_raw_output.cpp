#include "ydwg_raw_output.h"
#include "time_string.h"
#include "origin_string.h"

OriginString CANFrameToYDWGRaw(const CANFrame& frame, struct timeval& timestamp) {
  constexpr int kBufferSize = 32;
  char buffer[kBufferSize];

  String time_str = GetUTCTimeString(timestamp);

  // get the CAN id as a hex string
  snprintf(buffer, kBufferSize, "%08X ", frame.id);
  String can_id_str = String(buffer);

  // get the CAN data as a hex bytes
  for (int i = 0; i < frame.len; i++) {
    snprintf(buffer + 3*i, kBufferSize - 3*i, "%02X ", frame.buf[i]);
  }
  // replace the final space with a zero
  buffer[3*frame.len - 1] = '\0';

  char direction = frame.origin_type == CANFrameOriginType::kApp ? 'T' : 'R';

  String out = time_str + direction + " " + can_id_str + buffer + "\r\n";

  OriginString origin_string = {frame.origin_id, out};

  return origin_string;
}
