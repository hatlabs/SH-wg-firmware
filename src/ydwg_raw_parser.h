#ifndef SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_
#define SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_

#include <Arduino.h>
#include <N2kMsg.h>

#include "can_frame.h"

bool YDWGRawToCANFrame(CANFrame& frame, const String& ydwg_raw, struct timeval& timestamp);

#endif  // SH_WG_FIRMWARE_YDWG_RAW_PARSER_H_
