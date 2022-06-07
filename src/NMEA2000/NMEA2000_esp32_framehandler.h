#ifndef SH_WG_FIRMWARE_NMEA2000_NMEA2000_ESP32_FRAMEHANDLER_H_
#define SH_WG_FIRMWARE_NMEA2000_NMEA2000_ESP32_FRAMEHANDLER_H_

#include "NMEA2000_esp32.h"

/**
 * @brief tNMEA2000_esp32 class with frame handler callback support.
 *
 */
class tNMEA2000_esp32_FH : public tNMEA2000_esp32 {
 public:
  tNMEA2000_esp32_FH(gpio_num_t _TxPin = ESP32_CAN_TX_PIN,
                     gpio_num_t _RxPin = ESP32_CAN_RX_PIN)
      : tNMEA2000_esp32(_TxPin, _RxPin) {}

  void SetCANFrameHandler(void (*_FrameHandler)(bool& hasFrame,
                                                unsigned long& canId,
                                                unsigned char& len,
                                                unsigned char* buf));

  // expose CANSendFrame to public
  bool CANSendFrame(unsigned long id, unsigned char len,
                    const unsigned char* buf, bool wait_sent = true) {
    // call parent CANSendFrame
    return tNMEA2000_esp32::CANSendFrame(id, len, buf, wait_sent);
  }

 protected:
  bool CANGetFrame(unsigned long& id, unsigned char& len, unsigned char* buf);

  void (*CANFrameHandler)(bool& hasFrame, unsigned long& canId,
                          unsigned char& len, unsigned char* buf);
  void RunCANFrameHandlers(bool& hasFrame, unsigned long& canId,
                           unsigned char& len, unsigned char* buf);
};

#endif  // SH_WG_FIRMWARE_NMEA2000_NMEA2000_ESP32_FRAMEHANDLER_H_
