// config.json(LittleFS)の読み込みと保存
#pragma once
#include <Arduino.h>
#include <vector>

struct ScanConfig {
  uint32_t interval_sec = 15;      // 30秒では入店時に間に合わなかった(2026-11-23)
  uint32_t confirm_after_sec = 3;
  uint8_t enter_hits = 2;
  uint8_t exit_misses = 3;
};

struct LawsonConfig {
  std::vector<String> ssid_prefixes{"LAWSON_Free_Wi-Fi", "PM_TEST_LAWSON"};
  int rssi_min = -80;
};

struct CardConfig {
  String label = "Ponta";
  String format = "NW7";
  String value = "A000000000000000A";  // ダミー。本物は config.json にだけ置く
  int narrow_px = 3;
  int wide_px = 8;
};

struct ProfileConfig {
  String name = "(表示名)";
  String handle = "@kgsbl";
  String bio = "(紹介文)";
  String url = "https://x.com/kgsbl";
  String icon = "/icon.png";
};

struct PowerConfig {
  bool light_sleep = true;  // USBシリアル接続中は自動で無効
};

struct AppConfig {
  ScanConfig scan;
  LawsonConfig lawson;
  CardConfig card;
  ProfileConfig profile;
  PowerConfig power;
  bool loaded_from_file = false;
};

extern AppConfig g_config;

constexpr const char* kConfigPath = "/config.json";

// LittleFSの /config.json を読む。無ければ既定値のまま false
bool loadConfig();
// JSON文字列を検証して /config.json に保存し、読み直す
bool saveConfigJson(const String& json, String& err);
// 現在のファイル内容をシリアルに出す
void printConfig(Stream& out);
