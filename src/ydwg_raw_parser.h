#ifndef SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_
#define SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_

#include <Arduino.h>
#include <N2kMsg.h>
#include <sys/time.h>

#include "can_frame.h"
#include "sensesp/transforms/transform.h"

using namespace sensesp;

bool YDWGRawToCANFrame(CANFrame& frame, char& direction,
                       struct timeval& timestamp, const String& ydwg_raw);

class YDWGRawToCANFrameTransform : public Transform<String, CANFrame> {
 public:
  YDWGRawToCANFrameTransform() : Transform<String, CANFrame>() {}

  void set_input(const String ydwg_raw_str, uint8_t input_channel) override {
    CANFrame frame;
    char direction;
    struct timeval timestamp;
    if (YDWGRawToCANFrame(frame, direction, timestamp, ydwg_raw_str)) {
      emit(frame);
    } else {
      debugW("YDWG RAW string parsing failed: %s", ydwg_raw_str.c_str());
    }
  }
};

#endif  // SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_
