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

  void set(const OriginString& ydwg_raw_str) override {
    CANFrame frame;
    char direction;
    struct timeval timestamp;
    if (YDWGRawToCANFrame(frame, timestamp, ydwg_raw_str)) {
      emit(frame);
    }
  }
};

#endif  // SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_
