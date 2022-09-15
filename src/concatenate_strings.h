#ifndef SH_WG_FIRMWARE_CONCATENATE_STRINGS_H_
#define SH_WG_FIRMWARE_CONCATENATE_STRINGS_H_

#include "elapsedMillis.h"
#include "sensesp/transforms/transform.h"
#include "shwg.h"

using namespace sensesp;

/**
 * @brief Concatenate input of successive calls to longer strings.
 *
 * ConcatenateStrings takes the input strings and concatenates them together,
 * as long as the total length of the string is less than the maximum
 * length specified in the constructor and the time since the first
 * input is less than the timeout specified in the constructor.
 */
class ConcatenateStrings : public Transform<String, String> {
 public:
  ConcatenateStrings(int max_delay, size_t max_length)
      : Transform<String, String>(),
        max_delay_{max_delay},
        max_length_{max_length} {
          ReactESP::app->onRepeat(1, [this]() { this->check_timeout(); });
        }

  void set_input(const String new_value, uint8_t input_channel) override {
    if (new_value.length() > max_length_) {
      debugW("Input string longer than max length: %s", new_value.c_str());
      return;
    }
    if (out_buffer_.length() == 0) {
      // This is the first input, so reset the timeout.
      buf_input_time_ = 0;
    }
    if (out_buffer_.length() + new_value.length() > max_length_) {
      // The new input would cause the buffer to exceed the max length,
      // so emit the current buffer and start a new one.
      emit(out_buffer_);
      out_buffer_ = new_value;
      buf_input_time_ = 0;
    } else {
      // The new input can be added to the buffer.
      out_buffer_ += new_value;
    }
  }

 private:
  int max_delay_;
  size_t max_length_;
  String out_buffer_ = "";
  elapsedMillis buf_input_time_ = 0;  //< Time since first input to buffer

  /**
   * @brief Check if the buffer has timed out and should be emitted.
   */
  void check_timeout() {
    if (buf_input_time_ > max_delay_) {
      if (out_buffer_.length() > 0) {
        emit(out_buffer_);
        out_buffer_ = "";
      }
    }
  }

};

#endif
