#include "sensesp/transforms/transform.h"

#include "origin_string.h"

namespace sensesp {

/**
 * @brief Transform that splits an OriginString into substrings based on a
 * delimiter.
 */
class StringTokenizer : public SymmetricTransform<OriginString> {
 public:
  StringTokenizer(String delimiter, String config_path = "")
      : SymmetricTransform<OriginString>(config_path), delimiter_{delimiter} {
    this->load_configuration();
  }
  void set_input(OriginString value, uint8_t input_channel) override {
    int pos;
    String value_string = value.data;
    while ((pos = value_string.indexOf(delimiter_)) != -1) {
      String substring = value_string.substring(0, pos);
      OriginString output = {value.origin_id, substring};
      this->emit(output);
      value_string = value_string.substring(pos + delimiter_.length());
    }
    // if there is anything left, emit it
    if (value_string.length() > 0) {
      OriginString output = {value.origin_id, value_string};
      this->emit(output);
    }
  }

 protected:
  String delimiter_;
};

}  // namespace sensesp
