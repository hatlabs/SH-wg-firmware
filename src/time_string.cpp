#include "time_string.h"

#include <Arduino.h>

/**
 * @brief Convert a timeval struct to String.
 *
 * @param timestamp
 * @return const String
 */
const String GetUTCTimeString(const struct timeval& timestamp) {
  static constexpr int kBufferSize = 32;
  char buffer[kBufferSize];
  struct tm tm_info;

  // convert the timestamp into a time struct
  gmtime_r(&timestamp.tv_sec, &tm_info);

  // output the time as a formatted string
  snprintf(buffer, kBufferSize, "%02d:%02d:%02d.%03ld ", tm_info.tm_hour,
           tm_info.tm_min, tm_info.tm_sec, timestamp.tv_usec / 1000);

  return String(buffer);
}

/**
 * @brief Get the current system time as an UTC string.
 *
 * @return const String
 */
const String GetSystemUTCTimeString() {
  struct timeval tv_now;

  // get current time as a timestamp
  gettimeofday(&tv_now, NULL);

  return GetUTCTimeString(tv_now);
}
