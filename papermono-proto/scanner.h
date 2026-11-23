// Wi-Fiスキャン・ローソン判定・スキャンログ(CSV)
#pragma once
#include <Arduino.h>

struct ScanResult {
  bool ok = false;       // スキャン自体が成功したか
  bool matched = false;  // ローソンと判定したか
  int best_rssi = -127;  // 一致したAPのうち最も強いRSSI
  String best_ssid;
  int ap_count = 0;
};

void scannerBegin();
// スキャンを1回行い、結果をログに追記する(ブロッキング、数秒かかる)
ScanResult scanOnce();

// シリアル用
void scanlogDump(Stream& out);
void scanlogClear();
size_t scanlogSize();
