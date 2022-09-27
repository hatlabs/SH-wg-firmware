#include "ui_controls.h"

static const char kPortConfigSchema[] = R"({
    "type": "object",
    "properties": {
        "enable": { "title": "Enable", "type": "boolean" },
        "port": { "title": "Port", "type": "integer" }
    }
  })";

String PortConfig::get_config_schema() { return kPortConfigSchema; }

void PortConfig::get_configuration(JsonObject& root) {
  root["enable"] = enabled_;
  root["port"] = port_;
}

bool PortConfig::set_configuration(const JsonObject& config) {
  if (!config.containsKey("enable")) {
    return false;
  } else {
    enabled_ = config["enable"];
  }

  if (!config.containsKey("port")) {
    return false;
  } else {
    port_ = config["port"];
  }

  return true;
}


static const char kBiDiPortConfigSchemaTemplate[] = R"({
    "type": "object",
    "properties": {
        "enable_tx": { "title": "{{tx_title}}", "type": "boolean" },
        "enable_rx": { "title": "{{rx_title}}", "type": "boolean" },
        "port": { "title": "Port", "type": "integer" }
    }
  })";

String BiDiPortConfig::get_config_schema() {
  String schema = kBiDiPortConfigSchemaTemplate;
  schema.replace("{{tx_title}}", tx_title_);
  schema.replace("{{rx_title}}", rx_title_);
  return schema; }

void BiDiPortConfig::get_configuration(JsonObject& root) {
  root["enable_tx"] = tx_enabled_;
  root["enable_rx"] = rx_enabled_;
  root["port"] = port_;
}

bool BiDiPortConfig::set_configuration(const JsonObject& config) {
  if (!config.containsKey("enable_tx")) {
    return false;
  } else {
    tx_enabled_ = config["enable_tx"];
  }

  if (!config.containsKey("enable_rx")) {
    return false;
  } else {
    rx_enabled_ = config["enable_rx"];
  }

  if (!config.containsKey("port")) {
    return false;
  } else {
    port_ = config["port"];
  }

  return true;
}

static const char kHostPortConfigSchemaTemplate[] = R"({
    "type": "object",
    "properties": {
        "enable": { "title": "{{title}}", "type": "boolean" },
        "host": { "title": "{{host}}", "type": "string" },
        "port": { "title": "{{port}}", "type": "integer" }
    }
  })";

String HostPortConfig::get_config_schema() {
  String schema = kHostPortConfigSchemaTemplate;
  schema.replace("{{title}}", enabled_title_);
  schema.replace("{{host}}", host_title_);
  schema.replace("{{port}}", port_title_);
  return schema;
}

void HostPortConfig::get_configuration(JsonObject& root) {
  root["enable"] = enabled_;
  root["host"] = host_;
  root["port"] = port_;
}

bool HostPortConfig::set_configuration(const JsonObject& config) {
  if (!config.containsKey("enable")) {
    return false;
  } else {
    enabled_ = config["enable"];
  }

  if (!config.containsKey("host")) {
    return false;
  } else {
    host_ = config["host"].as<String>();
  }

  if (!config.containsKey("port")) {
    return false;
  } else {
    port_ = config["port"];
  }

  return true;
}

static const char kCheckboxConfigSchemaTemplate[] = R"({
    "type": "object",
    "properties": {
        "value": { "title": "{{title}}", "type": "boolean" }
    }
  })";

String CheckboxConfig::get_config_schema() {
  String schema = kCheckboxConfigSchemaTemplate;
  schema.replace("{{title}}", title_);
  return schema;
}

void CheckboxConfig::get_configuration(JsonObject& root) {
  root["value"] = value_;
}

bool CheckboxConfig::set_configuration(const JsonObject& config) {
  if (!config.containsKey("value")) {
    return false;
  } else {
    value_ = config["value"];
  }

  return true;
}

static const char kStringConfigSchemaTemplate[] = R"({
    "type": "object",
    "properties": {
        "value": { "title": "{{title}}", "type": "string" }
    }
  })";

String StringConfig::get_config_schema() {
  String schema = kStringConfigSchemaTemplate;
  schema.replace("{{title}}", title_);
  return schema;
}

void StringConfig::get_configuration(JsonObject& root) {
  root["value"] = value_;
}

bool StringConfig::set_configuration(const JsonObject& config) {
  if (!config.containsKey("value")) {
    return false;
  } else {
    value_ = config["value"].as<String>();
  }

  return true;
}


