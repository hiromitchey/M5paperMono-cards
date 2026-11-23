// PROFILE / LAWSON 画面の描画
#pragma once
#include <Arduino.h>

struct StatusInfo {
  int battery = -1;          // %(取れなければ -1)
  String last_scan;          // 例 "12:34" や "up 123s"
  int lawson_hits = 0;       // 連続検知回数(デバッグ用)
  int ap_count = -1;
};

struct LawsonInfo {
  int rssi = 0;              // 0 なら非表示
  bool manual = false;       // ボタンで出した
};

void screensBegin();
void drawProfile(const StatusInfo& st);
// ステータス行だけ部分更新。10回ごとに全面描き換え
void updateProfileStatus(const StatusInfo& st);
void drawLawson(const LawsonInfo& info);
// 残像消し(白黒反転を挟んで全面描き換え)
void fullRefresh();
