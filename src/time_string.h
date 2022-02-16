#ifndef SH_WG_FIRMWARE_TIME_STRING_H_
#define SH_WG_FIRMWARE_TIME_STRING_H_

#include <Arduino.h>

#include <sys/time.h>

const String GetUTCTimeString(const struct timeval& timestamp);
const String GetSystemUTCTimeString();

#endif
