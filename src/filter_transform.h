#include "sensesp/transforms/transform.h"

namespace sensesp {

/**
 * @brief Transform that emits its input if the provided function returns true.
 *
 * @tparam T
 */
template <class T>
class Filter : public SymmetricTransform<T> {
 public:
  Filter(std::function<bool(T)> function, String config_path = "")
      : SymmetricTransform<T>(config_path), function_{function} {
    this->load_configuration();
  }
  void set_input(T value, uint8_t input_channel) override {
    if (function_(value)) {
      this->emit(value);
    }
  }
 protected:
  std::function<bool(T)> function_;
};

}  // namespace sensesp
