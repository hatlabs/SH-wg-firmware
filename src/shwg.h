#ifndef SHWG_SHWG_H_
#define SHWG_SHWG_H_

#include "sensesp_minimal_app.h"

extern reactesp::ReactESP app;
extern sensesp::SensESPMinimalApp *sensesp_app;

String MacAddrToString(uint8_t *mac, bool add_colons = false);
void PrintProductInfo();

// template function that returns a pointer cast to uint32_t
template <typename T>
uint32_t origin_id(T *ptr) {
  return reinterpret_cast<uint32_t>(ptr);
}

#endif
