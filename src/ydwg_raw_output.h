#ifndef SH_WG_FIRMWARE_YDWG_RAW_OUTPUT_H_
#define SH_WG_FIRMWARE_YDWG_RAW_OUTPUT_H_

#include <Arduino.h>
#include <N2kMsg.h>

#include "can_frame.h"
#include "origin_string.h"

OriginString CANFrameToYDWGRaw(const CANFrame& frame, struct timeval& timestamp);

#endif  // SH_WG_FIRMWARE_YDWG_RAW_OUTPUT_H_
