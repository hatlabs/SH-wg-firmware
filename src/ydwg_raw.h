#ifndef SH_WG_FIRMWARE_YDWG_RAW_H_
#define SH_WG_FIRMWARE_YDWG_RAW_H_

#include <Arduino.h>
#include <N2kMsg.h>

#include "can_frame.h"

String CANFrameToYDWGRaw(const CANFrame& frame, struct timeval& timestamp);

#endif  // SH_WG_FIRMWARE_YDWG_RAW_H_
