#ifndef SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_
#define SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_

#include <Arduino.h>
#include <N2kMsg.h>
#include <sys/time.h>

#include "can_frame.h"
#include "origin_string.h"
#include "sensesp/transforms/transform.h"

using namespace sensesp;

bool YDWGRawToCANFrame(CANFrame& frame, struct timeval& timestamp,
                       const OriginString& ydwg_raw);

class YDWGRawToCANFrameTransform : public Transform<OriginString, CANFrame> {
 public:
  YDWGRawToCANFrameTransform() : Transform<OriginString, CANFrame>() {}

  void set_input(const OriginString ydwg_raw_str,
                 uint8_t input_channel) override {
    CANFrame frame;
    char direction;
    struct timeval timestamp;
    if (YDWGRawToCANFrame(frame, timestamp, ydwg_raw_str)) {
      emit(frame);
    } else {
      debugW("YDWG RAW string parsing failed: %s", ydwg_raw_str.data.c_str());
    }
  }
};

#endif  // SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_
