#include "config.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

AppConfig g_config;

namespace {
template <typename T>
void readIf(JsonVariantConst v, T& dst) {
  if (!v.isNull() && v.is<T>()) dst = v.as<T>();
}

void readStr(JsonVariantConst v, String& dst) {
  if (v.is<const char*>()) dst = v.as<const char*>();
}

bool parseInto(const String& json, AppConfig& c, String& err) {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, json);
  if (e) {
    err = String("JSONの解析に失敗: ") + e.c_str();
    return false;
  }
  JsonObjectConst root = doc.as<JsonObjectConst>();
  if (root.isNull()) {
    err = "JSONのトップがオブジェクトではない";
    return false;
  }

  JsonObjectConst s = root["scan"];
  readIf(s["interval_sec"], c.scan.interval_sec);
  readIf(s["confirm_after_sec"], c.scan.confirm_after_sec);
  readIf(s["enter_hits"], c.scan.enter_hits);
  readIf(s["exit_misses"], c.scan.exit_misses);

  JsonObjectConst l = root["lawson"];
  if (l["ssid_prefixes"].is<JsonArrayConst>()) {
    c.lawson.ssid_prefixes.clear();
    for (JsonVariantConst p : l["ssid_prefixes"].as<JsonArrayConst>()) {
      if (p.is<const char*>()) c.lawson.ssid_prefixes.emplace_back(p.as<const char*>());
    }
  }
  readIf(l["rssi_min"], c.lawson.rssi_min);

  JsonObjectConst k = root["card"];
  readStr(k["label"], c.card.label);
  readStr(k["format"], c.card.format);
  readStr(k["value"], c.card.value);
  readIf(k["narrow_px"], c.card.narrow_px);
  readIf(k["wide_px"], c.card.wide_px);

  JsonObjectConst p = root["profile"];
  readStr(p["name"], c.profile.name);
  readStr(p["handle"], c.profile.handle);
  readStr(p["bio"], c.profile.bio);
  readStr(p["url"], c.profile.url);
  readStr(p["icon"], c.profile.icon);

  JsonObjectConst w = root["power"];
  readIf(w["light_sleep"], c.power.light_sleep);

  // 最低限の値チェック
  if (c.scan.interval_sec < 5) c.scan.interval_sec = 5;
  if (c.scan.enter_hits < 1) c.scan.enter_hits = 1;
  if (c.scan.exit_misses < 1) c.scan.exit_misses = 1;
  if (c.card.narrow_px < 1) c.card.narrow_px = 1;
  if (c.card.wide_px <= c.card.narrow_px) c.card.wide_px = c.card.narrow_px * 2 + 2;
  return true;
}
}  // namespace

bool loadConfig() {
  File f = LittleFS.open(kConfigPath, "r");
  if (!f) return false;
  String json = f.readString();
  f.close();
  AppConfig c;
  String err;
  if (!parseInto(json, c, err)) {
    Serial.printf("[config] %s\n", err.c_str());
    return false;
  }
  c.loaded_from_file = true;
  g_config = c;
  return true;
}

bool saveConfigJson(const String& json, String& err) {
  AppConfig c;
  if (!parseInto(json, c, err)) return false;
  File f = LittleFS.open(kConfigPath, "w");
  if (!f) {
    err = "config.json を開けない";
    return false;
  }
  f.print(json);
  f.close();
  return loadConfig();
}

void printConfig(Stream& out) {
  File f = LittleFS.open(kConfigPath, "r");
  if (!f) {
    out.println("(config.json なし。既定値で動作中)");
    return;
  }
  while (f.available()) out.write(f.read());
  out.println();
  f.close();
}
