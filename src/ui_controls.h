#ifndef SH_WG_SRC_UI_CONTROLS_H_
#define SH_WG_SRC_UI_CONTROLS_H_

#include "sensesp.h"
#include "sensesp/system/configurable.h"

using namespace sensesp;

/**
 * @brief Configurable with Enable checkbox and a Port input field.
 *
 */
class PortConfig : public Configurable {
 public:
  PortConfig(bool enabled, uint16_t port, String config_path,
             String description, int sort_order = 1000)
      : enabled_(enabled),
        port_(port),
        Configurable(config_path, description, sort_order) {
    load_configuration();
  }

  virtual void get_configuration(JsonObject& doc) override;
  virtual bool set_configuration(const JsonObject& config) override;
  virtual String get_config_schema() override;

  bool get_enabled() { return enabled_; }
  uint16_t get_port() { return port_; }

 protected:
  bool enabled_ = false;
  int port_ = 0;
};

class BiDiPortConfig : public Configurable {
 public:
  BiDiPortConfig(bool tx_enabled, bool rx_enabled, String tx_title,
                 String rx_title, uint16_t port, String config_path,
                 String description, int sort_order = 1000)
      : tx_enabled_(tx_enabled),
        rx_enabled_(rx_enabled),
        port_(port),
        Configurable(config_path, description, sort_order) {
    load_configuration();
  }

  virtual void get_configuration(JsonObject& doc) override;
  virtual bool set_configuration(const JsonObject& config) override;
  virtual String get_config_schema() override;

  bool get_tx_enabled() { return tx_enabled_; }
  bool get_rx_enabled() { return rx_enabled_; }
  uint16_t get_port() { return port_; }

 protected:
  bool tx_enabled_ = false;
  bool rx_enabled_ = false;

  String tx_title_ = "Transmit";
  String rx_title_ = "Receive";

  int port_ = 0;
};

class CheckboxConfig : public Configurable {
 public:
  CheckboxConfig(bool value, String title, String config_path,
                 String description, int sort_order = 1000)
      : value_(value),
        title_(title),
        Configurable(config_path, description, sort_order) {
    load_configuration();
  }

  virtual void get_configuration(JsonObject& doc) override;
  virtual bool set_configuration(const JsonObject& config) override;
  virtual String get_config_schema() override;

  bool get_value() { return value_; }

 protected:
  bool value_ = false;
  String title_ = "Enable";
};

class StringConfig : public Configurable {
 public:
  StringConfig(String value, String config_path, String description,
               int sort_order = 1000)
      : value_(value), Configurable(config_path, description, sort_order) {
    load_configuration();
  }

  virtual void get_configuration(JsonObject& doc) override;
  virtual bool set_configuration(const JsonObject& config) override;
  virtual String get_config_schema() override;

  String get_value() { return value_; }

 protected:
  String value_;
  String title_ = "Value";
};

#endif  // SH_WG_SRC_UI_CONTROLS_H_
