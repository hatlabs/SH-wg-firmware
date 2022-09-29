#ifndef SH_WG_FIRMWARE_ORIGIN_STRING_H_
#define SH_WG_FIRMWARE_ORIGIN_STRING_H_

#include <Arduino.h>

#include <cstdint>

#include "sensesp.h"

using namespace sensesp;

/**
 * @brief Container for Origin aware strings.
 */
struct OriginString {
  uint32_t origin_id;  // string origin identifier
  String data;         // data
};

#endif  // SH_WG_FIRMWARE_ORIGIN_STRING_H_
