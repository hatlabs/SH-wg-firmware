#ifndef SH_WG_SRC_UI_CONTROLS_H_
#define SH_WG_SRC_UI_CONTROLS_H_

#include "sensesp.h"
#include "sensesp/ui/config_item.h"

using namespace sensesp;

/**
 * @brief Configurable with Enable checkbox and a Port input field.
 *
 */
class PortConfig : public FileSystemSaveable {
 public:
  PortConfig(bool enabled, uint16_t port, String config_path)
      : enabled_(enabled),
        port_(port),
        FileSystemSaveable(config_path) {
    load();
  }

  virtual bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& config) override;

  bool get_enabled() { return enabled_; }
  uint16_t get_port() { return port_; }

 protected:
  bool enabled_ = false;
  int port_ = 0;
};

static const char kPortConfigSchema[] = R"({
    "type": "object",
    "properties": {
        "enable": { "title": "Enable", "type": "boolean" },
        "port": { "title": "Port", "type": "integer" }
    }
  })";

inline const String ConfigSchema(const PortConfig& obj) {
  return kPortConfigSchema;
}

class BiDiPortConfig : public FileSystemSaveable {
 public:
  BiDiPortConfig(bool tx_enabled, bool rx_enabled, String tx_title,
                 String rx_title, uint16_t port, String config_path)
      : tx_enabled_(tx_enabled),
        rx_enabled_(rx_enabled),
        tx_title_(tx_title),
        rx_title_(rx_title),
        port_(port),
        FileSystemSaveable(config_path) {
    load();
  }

  virtual bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& config) override;

  bool get_tx_enabled() { return tx_enabled_; }
  bool get_rx_enabled() { return rx_enabled_; }
  uint16_t get_port() { return port_; }

 protected:
  bool tx_enabled_ = false;
  bool rx_enabled_ = false;

  String tx_title_ = "Transmit";
  String rx_title_ = "Receive";

  int port_ = 0;
  friend const String ConfigSchema(const BiDiPortConfig& obj);
};

static const char kBiDiPortConfigSchemaTemplate[] = R"({
    "type": "object",
    "properties": {
        "enable_tx": { "title": "{{tx_title}}", "type": "boolean" },
        "enable_rx": { "title": "{{rx_title}}", "type": "boolean" },
        "port": { "title": "Port", "type": "integer" }
    }
  })";

const inline String ConfigSchema(const BiDiPortConfig& obj) {
  String schema = kBiDiPortConfigSchemaTemplate;
  schema.replace("{{tx_title}}", obj.tx_title_);
  schema.replace("{{rx_title}}", obj.rx_title_);
  return schema.c_str();
}

class HostPortConfig : public FileSystemSaveable {
 public:
  HostPortConfig(bool enabled, String host, uint16_t port, String enabled_title,
    String host_title, String port_title, String config_path)
      : enabled_(enabled),
        host_(host),
        port_(port),
        enabled_title_(enabled_title),
        host_title_(host_title),
        port_title_(port_title),
        FileSystemSaveable(config_path) {
    load();
  }

  virtual bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& config) override;

  bool get_enabled() { return enabled_; }
  String get_host() { return host_; }
  uint16_t get_port() { return port_; }

 protected:
  bool enabled_ = false;
  String host_ = "";
  int port_ = 0;
  String enabled_title_;
  String host_title_;
  String port_title_;
  friend const String ConfigSchema(const HostPortConfig& obj);
};

static const char kHostPortConfigSchemaTemplate[] = R"({
    "type": "object",
    "properties": {
        "enable": { "title": "{{title}}", "type": "boolean" },
        "host": { "title": "{{host}}", "type": "string" },
        "port": { "title": "{{port}}", "type": "integer" }
    }
  })";

const inline String ConfigSchema(const HostPortConfig& obj) {
  String schema = kHostPortConfigSchemaTemplate;
  schema.replace("{{title}}", obj.enabled_title_);
  schema.replace("{{host}}", obj.host_title_);
  schema.replace("{{port}}", obj.port_title_);
  return schema.c_str();
}

#endif  // SH_WG_SRC_UI_CONTROLS_H_
