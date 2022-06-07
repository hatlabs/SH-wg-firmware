#include "ydwg_raw_parser.h"

#include <sys/time.h>

#include "shwg.h"

// Workaround for getting strptime to work with the Arduino framework
extern "C" char *strptime (const char *__restrict, const char *__restrict, struct tm *__restrict);

using namespace sensesp;

/**
 * @brief Parse a YDWG raw string into a CAN frame.
 *
 * @param frame Destination CAN frame.
 * @param timestamp Destination timestamp.
 * @param ydwg_raw Source raw string
 * @return true Parsing was successful.
 * @return false Parsing failed.
 */
bool YDWGRawToCANFrame(CANFrame& frame, char& direction,
                       struct timeval& timestamp, const String& ydwg_raw) {
  // Example YDWG raw string:
  // 15:53:34.738 R 0DFF0600 20 0F 13 99 FF 01 00 0B

  // Maximum length of a YDWG raw string, including CRLF.
  constexpr int kMaxLength = 49;

  // Check if the string is too long.
  if (ydwg_raw.length() > kMaxLength) {
    debugD("YDWG raw string too long: %d", ydwg_raw.length());
    return false;
  }

  char buf[kMaxLength];

  // Copy the string to a buffer.
  ydwg_raw.toCharArray(buf, kMaxLength);

  // Remove the CRLF.
  buf[ydwg_raw.length() - 2] = '\0';

  // get the timestamp string

  char* time_str = strtok(buf, " \t");

  if (time_str == NULL) {
    debugD("Timestamp token parsing failed: %s", buf);
    return false;
  }

  // verify the timestamp string length
  if (strlen(time_str) != 12) {
    debugD("Timestamp string length incorrect: %s", time_str);
    return false;
  }

  // Parse the timestamp string into a timeval struct
  int hour;
  int minute;
  float second;
  if (sscanf(time_str, "%d:%d:%f", &hour, &minute, &second) != 3) {
    debugD("Timestamp string parsing failed: %s", time_str);
    return false;
  }

  if (hour < 0 || hour > 23) {
    debugD("Hour out of range: %d", hour);
    return false;
  }
  if (minute < 0 || minute > 59) {
    debugD("Minute out of range: %d", minute);
    return false;
  }
  if (second < 0.0 || second >= 60.0) {
    debugD("Second out of range: %f", second);
    return false;
  }

  timestamp.tv_sec = hour * 3600 + minute * 60 + (int)second;
  timestamp.tv_usec = (int)((second - (int)second) * 1000000);

  // get the direction token
  char* dir_token = strtok(NULL, " \t");

  // verify the token string length
  if (strlen(dir_token) != 1 || (dir_token[0] != 'R' && dir_token[0] != 'T')) {
    debugD("Direction token length incorrect: %s", dir_token);
    return false;
  }

  direction = dir_token[0];

  // get the CAN id token
  char* can_id_token = strtok(NULL, " \t");

  // verify the token string length
  int can_id_token_length = strlen(can_id_token);
  if (can_id_token_length == 0 || can_id_token_length > 8) {
    debugD("CAN id token length incorrect: %s", can_id_token);
    return false;
  }

  // convert the can_id_token to a uint32_t
  uint32_t can_id = 0;
  if (sscanf(can_id_token, "%x", &can_id) != 1) {
    debugD("CAN id token parsing failed: %s", can_id_token);
    return false;
  }

  char data[8];
  int data_length = 0;

  // collect the data bytes
  for (int i = 0; i < 8; i++) {
    char* data_token = strtok(NULL, " \t");

    if (data_token == NULL) {
      // we've reached the end of the data tokens
      break;
    }

    // verify the token string length
    if (strlen(data_token) != 2) {
      debugD("Data token length incorrect: %s", data_token);
      return false;
    }

    // convert the data_token to a uint8_t
    uint8_t data_byte;
    if (sscanf(data_token, "%02x", &data_byte) != 1) {
      debugD("Data token parsing failed: %s", data_token);
      return false;
    }

    data[i] = data_byte;
    data_length++;
  }

  // set the CAN frame contents
  frame.id = can_id;
  frame.len = data_length;
  memcpy(frame.buf, data, data_length);

  return true;
}
