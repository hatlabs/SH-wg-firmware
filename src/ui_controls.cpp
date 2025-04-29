#include "ui_controls.h"


bool PortConfig::to_json(JsonObject& root) {
  root["enable"] = enabled_;
  root["port"] = port_;
  return true;
}

bool PortConfig::from_json(const JsonObject& config) {
  String const expected[] = {"enable", "port"};
  for (auto str : expected) {
    if (!config[str].is<JsonVariant>()) {
      return false;
    }
  }
  enabled_ = config["enable"];
  port_ = config["port"];
  return true;
}

bool BiDiPortConfig::to_json(JsonObject& root) {
  root["enable_tx"] = tx_enabled_;
  root["enable_rx"] = rx_enabled_;
  root["port"] = port_;
  return true;
}

bool BiDiPortConfig::from_json(const JsonObject& config) {
  String const expected[] = {"enable_tx", "enable_rx", "port"};
  for (auto str : expected) {
    if (!config[str].is<JsonVariant>()) {
      return false;
    }
  }
  tx_enabled_ = config["enable_tx"];
  rx_enabled_ = config["enable_rx"];
  port_ = config["port"];
  return true;
}


bool HostPortConfig::to_json(JsonObject& root) {
  root["enable"] = enabled_;
  root["host"] = host_;
  root["port"] = port_;
  return true;
}

bool HostPortConfig::from_json(const JsonObject& config) {
  String const expected[] = {"enable", "host", "port"};
  for (auto str : expected) {
    if (!config[str].is<JsonVariant>()) {
      return false;
    }
  }
  enabled_ = config["enable"];
  host_ = config["host"].as<String>();
  port_ = config["port"];
  return true;
}