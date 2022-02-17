#ifndef SH_WG_FIRMWARE_CAN_FRAME_H_
#define SH_WG_FIRMWARE_CAN_FRAME_H_

#include <cstdint>

/**
 * @brief Container for CAN frame data.
 *
 */
struct CANFrame {
  uint32_t id;  // can identifier
  uint8_t len;  // length of data
  uint8_t buf[8];
};

#endif // SH_WG_FIRMWARE_CAN_FRAME_H_
