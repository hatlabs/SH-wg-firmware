#include "ydwg_raw_parser.h"

#include <sys/time.h>

#include "can_frame.h"
#include "origin_string.h"
#include "shwg.h"

// Workaround for getting strptime to work with the Arduino framework
extern "C" char* strptime(const char* __restrict, const char* __restrict,
                          struct tm* __restrict);

using namespace sensesp;

String next_token(const String& str, int& pos) {
  // find the next delimiter
  int delim_pos = str.indexOf(' ', pos);
  if (delim_pos == -1) {
    delim_pos = str.length();
  }
  String token = str.substring(pos, delim_pos);
  pos = delim_pos + 1;
  return token;
}

bool YDWGRawAppStringToCANFrame(CANFrame& frame,
                                const OriginString ydwg_raw_app) {
  int pos = 0;

  String ydwg_raw_app_str = ydwg_raw_app.data;

  // YDWG RAW app messages need to be resent to the origin; let's
  // clear the origin id to do that.
  frame.origin_id = 0;

  // get the CAN id token
  String can_id_token = next_token(ydwg_raw_app_str, pos);

  // verify the token string length
  int can_id_token_length = can_id_token.length();
  if (can_id_token_length == 0 || can_id_token_length > 8) {
    debugD("CAN id token length incorrect: %s", can_id_token.c_str());
    return false;
  }

  // convert the can_id_token to a uint32_t
  uint32_t can_id = 0;
  if (sscanf(can_id_token.c_str(), "%x", &can_id) != 1) {
    debugD("CAN id token parsing failed: %s", can_id_token.c_str());
    return false;
  }

  char data[8];
  int data_length = 0;

  // collect the data bytes
  for (int i = 0; i < 8; i++) {
    String data_token = next_token(ydwg_raw_app_str, pos);

    if (data_token == "") {
      // we've reached the end of the data tokens
      break;
    }

    // verify the token string length
    if (data_token.length() != 2) {
      debugD("Data token length incorrect: %s (%d)", data_token.c_str(), pos);
      return false;
    }

    // convert the data_token to a uint8_t
    unsigned int data_byte;
    if (sscanf(data_token.c_str(), "%02x", &data_byte) != 1) {
      debugD("Data token parsing failed: %s", data_token.c_str());
      return false;
    }

    data[i] = data_byte;
    data_length++;
  }

  // set the CAN frame contents
  frame.id = can_id;
  frame.len = data_length;
  memcpy(frame.buf, data, data_length);
  frame.origin_type = CANFrameOriginType::kApp;

  return true;
}

bool YDWGRawDeviceStringToCANFrame(CANFrame& frame, struct timeval& timestamp,
                                   const OriginString ydwg_raw) {
  int pos = 0;

  String ydwg_raw_str = ydwg_raw.data;

  // get the timestamp string

  String time_str = next_token(ydwg_raw_str, pos);

  if (time_str == "") {
    debugD("Timestamp token parsing failed: %s", ydwg_raw_str.c_str());
    return false;
  }

  // verify the timestamp string length
  if (time_str.length() != 12) {
    // This would be triggered for all Application messages; don't
    // report an error to limit the amount of noise.
    return false;
  }

  // Parse the timestamp string into a timeval struct
  int hour;
  int minute;
  float second;
  if (sscanf(time_str.c_str(), "%d:%d:%f", &hour, &minute, &second) != 3) {
    debugD("Timestamp string parsing failed: %s", time_str.c_str());
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
  String dir_token = next_token(ydwg_raw_str, pos);

  // verify the token string length
  if (dir_token.length() != 1 || (dir_token[0] != 'R' && dir_token[0] != 'T')) {
    debugD("Direction token length incorrect: %s", dir_token.c_str());
    return false;
  }

  char direction = dir_token[0];

  // remaining substring should be identical to an YDWG RAW Application message

  String app_str = ydwg_raw_str.substring(pos);

  OriginString app_origin_str = {ydwg_raw.origin_id, app_str};

  if (YDWGRawAppStringToCANFrame(frame, app_origin_str)) {
    switch (direction) {
      case 'R':
        frame.origin_type = CANFrameOriginType::kRemoteCAN;
        break;
      case 'T':
        frame.origin_type = CANFrameOriginType::kRemoteApp;
        break;
      default:
        frame.origin_type = CANFrameOriginType::kUnknown;
        return false;
    }
  }

  frame.origin_id = ydwg_raw.origin_id;

  return true;
}

/**
 * @brief Parse a YDWG raw string into a CAN frame.
 *
 * @param frame Destination CAN frame.
 * @param timestamp Destination timestamp.
 * @param ydwg_raw Source raw string
 * @return true Parsing was successful.
 * @return false Parsing failed.
 */
bool YDWGRawToCANFrame(CANFrame& frame, struct timeval& timestamp,
                       const OriginString& ydwg_raw) {
  // Example YDWG raw string:
  // 15:53:34.738 R 0DFF0600 20 0F 13 99 FF 01 00 0B

  // Maximum length of a YDWG raw string, including CRLF.
  constexpr int kMaxLength = 49;

  // Check if the string is too long.
  if (ydwg_raw.data.length() > kMaxLength) {
    debugD("YDWG raw string too long: %d", ydwg_raw.data.length());
    return false;
  }

  OriginString ydwg_raw_copy = ydwg_raw;

  // Remove the CRLF.

  ydwg_raw_copy.data.trim();

  // Try parsing a Device format (timestamped) string.

  if (YDWGRawDeviceStringToCANFrame(frame, timestamp, ydwg_raw_copy)) {
    return true;
  }

  // Try parsing an App format (non-timestamped) string.

  if (YDWGRawAppStringToCANFrame(frame, ydwg_raw_copy)) {
    return true;
  }

  return false;
}
