#include "NMEA2000_esp32_framehandler.h"

/**
 * @brief Get a CAN frame from the CAN bus and pass it to the frame handler.
 *
 * The frame handler can modify the data in the CAN frame at will.
 * Besides replacing the data, it is even possible to inject new CAN frames
 * or delete the current one.
 *
 * @param id
 * @param len
 * @param buf
 * @return true A frame is available for further processing
 * @return false No frame was acquired
 */
bool tNMEA2000_esp32_FH::CANGetFrame(unsigned long &id, unsigned char &len,
                                     unsigned char *buf) {
  bool hasFrame = false;
  hasFrame = tNMEA2000_esp32::CANGetFrame(id, len, buf);

  RunCANFrameHandlers(hasFrame, id, len, buf);

  return hasFrame;
}

/**
 * @brief Run the frame handlers, possibly modifying the CAN frame.
 *
 * @param hasFrame
 * @param canId
 * @param len
 * @param buf
 */
void tNMEA2000_esp32_FH::RunCANFrameHandlers(bool &hasFrame, unsigned long &canId,
                                          unsigned char &len,
                                          unsigned char *buf) {
  if (CANFrameHandler != NULL) {
    CANFrameHandler(hasFrame, canId, len, buf);
  }
}

/**
 * @brief Set the frame handler.
 *
 * @param _FrameHandler
 */
void tNMEA2000_esp32_FH::SetCANFrameHandler(
    void (*_FrameHandler)(bool &hasFrame, unsigned long &canId,
                          unsigned char &len, unsigned char *buf)) {
  CANFrameHandler = _FrameHandler;
}
