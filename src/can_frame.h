#ifndef SH_WG_FIRMWARE_CAN_FRAME_H_
#define SH_WG_FIRMWARE_CAN_FRAME_H_

#include <cstdint>

/**
 * @brief CAN frame data origin enumeration.
 */
enum class CANFrameOrigin {
  kUnknown,   ///< Unknown origin.
  kLocal,     ///< Locally created.
  kApp,       ///< Received from an application.
  kRemoteCAN, ///< Received from another device, originating from CAN.
  kRemoteApp, ///< Received from another device, originating from an App.
  kCAN,       ///< Received from the CAN bus.
};

/**
 * @brief Container for CAN frame data.
 *
 */
struct CANFrame {
  uint32_t id;  // can identifier
  uint8_t len;  // length of data
  uint8_t buf[8];
  CANFrameOrigin origin;
};

#endif // SH_WG_FIRMWARE_CAN_FRAME_H_
