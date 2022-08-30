#ifndef SHWG_SHWG_H_
#define SHWG_SHWG_H_

#include "sensesp_minimal_app.h"

extern reactesp::ReactESP app;
extern sensesp::SensESPMinimalApp *sensesp_app;

String MacAddrToString(uint8_t *mac, bool add_colons = false);
void PrintProductInfo();

#endif
