#include "sensesp/transforms/transform.h"

namespace sensesp {

/**
 * @brief Transform that splits a string into substrings based on a delimiter.
 */
class StringTokenizer : public SymmetricTransform<String> {
 public:
  StringTokenizer(String delimiter, String config_path = "")
      : SymmetricTransform<String>(config_path), delimiter_{delimiter} {
    this->load_configuration();
  }
  void set_input(String value, uint8_t input_channel) override {
    int pos;
    while ((pos = value.indexOf(delimiter_)) != -1) {
      String substring = value.substring(0, pos);
      this->emit(substring);
      value = value.substring(pos + delimiter_.length());
    }
    // if there is anything left, emit it
    if (value.length() > 0) {
      this->emit(value);
    }
  }

 protected:
  String delimiter_;
};

}  // namespace sensesp
