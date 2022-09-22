#ifndef SH_WG_FIRMWARE_SEASMART_TRANSFORM_H_
#define SH_WG_FIRMWARE_SEASMART_TRANSFORM_H_

#include <NMEA0183.h>
#include <NMEA2000.h>

#include "ReactESP.h"
#include "Seasmart.h"
#include "config.h"
#include "elapsedMillis.h"
#include "origin_string.h"
#include "sensesp/transforms/transform.h"
#include "shwg.h"

using namespace sensesp;

String GetSeaSmartString(const tN2kMsg& n2k_msg) {
  char buf[kMaxNMEA2000MessageSeasmartSize];
  if (N2kToSeasmart(n2k_msg, millis(), buf, kMaxNMEA2000MessageSeasmartSize) ==
      0) {
    return "";
  } else {
    return String(buf) + "\r\n";
  }
}

class SeasmartTransform : public Transform<tN2kMsg, OriginString> {
 public:
  SeasmartTransform(tNMEA2000* nmea2000)
      : Transform<tN2kMsg, OriginString>(), nmea2000_{nmea2000} {}

  void set_input(tN2kMsg input, uint8_t input_channel = 0) override {
    String seasmart_str = GetSeaSmartString(input);
    // we're assuming that all tN2KMsg objects originate from nmea2000
    if (seasmart_str.length() > 0) {
      OriginString origin_str = {origin_id(nmea2000_), seasmart_str};
      this->emit(origin_str);
    }
  }

 protected:
  tNMEA2000* nmea2000_;  //< used to hardcode the origin
};

#endif  // SH_WG_FIRMWARE_N2K_NMEA0183_TRANSFORM_H_
