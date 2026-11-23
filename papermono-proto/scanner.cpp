#include "scanner.h"

#include <LittleFS.h>
#include <M5Unified.h>
#include <WiFi.h>

#include "config.h"

namespace {
constexpr const char* kLogPath = "/scanlog.csv";
constexpr const char* kLogOldPath = "/scanlog.old.csv";
constexpr size_t kLogMaxBytes = 512 * 1024;
constexpr const char* kLogHeader = "uptime_s,time,ssid,bssid,rssi,channel,matched";

bool isLawson(const String& ssid, int rssi) {
  if (rssi < g_config.lawson.rssi_min) return false;
  for (const auto& p : g_config.lawson.ssid_prefixes) {
    if (p.length() > 0 && ssid.startsWith(p)) return true;
  }
  return false;
}

// RTCが設定済みならISO8601、未設定なら空文字
String nowIso() {
  auto dt = M5.Rtc.getDateTime();
  if (dt.date.year < 2025) return "";
  char buf[24];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d", dt.date.year, dt.date.month,
           dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds);
  return buf;
}

// CSV用にカンマ・改行・ダブルクォートを含むSSIDをクォートする
String csvField(const String& s) {
  if (s.indexOf(',') < 0 && s.indexOf('"') < 0 && s.indexOf('\n') < 0) return s;
  String q = "\"";
  for (size_t i = 0; i < s.length(); ++i) {
    if (s[i] == '"') q += '"';
    q += s[i];
  }
  q += '"';
  return q;
}

void rotateIfNeeded() {
  File f = LittleFS.open(kLogPath, "r");
  if (!f) return;
  size_t sz = f.size();
  f.close();
  if (sz < kLogMaxBytes) return;
  LittleFS.remove(kLogOldPath);
  LittleFS.rename(kLogPath, kLogOldPath);
}
}  // namespace

void scannerBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
}

ScanResult scanOnce() {
  ScanResult r;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);  // つながない。スキャンだけ
  const int n = WiFi.scanNetworks(false, true);  // 同期・隠しSSIDも含める
  if (n < 0) {
    WiFi.mode(WIFI_OFF);
    return r;
  }
  r.ok = true;
  r.ap_count = n;

  rotateIfNeeded();
  const bool isNew = !LittleFS.exists(kLogPath);
  File f = LittleFS.open(kLogPath, "a");
  if (f && isNew) f.println(kLogHeader);

  const uint32_t up = millis() / 1000;
  const String ts = nowIso();
  for (int i = 0; i < n; ++i) {
    const String ssid = WiFi.SSID(i);
    const int rssi = WiFi.RSSI(i);
    const bool m = isLawson(ssid, rssi);
    if (m && rssi > r.best_rssi) {
      r.matched = true;
      r.best_rssi = rssi;
      r.best_ssid = ssid;
    }
    if (f) {
      f.printf("%lu,%s,%s,%s,%d,%ld,%d\n", (unsigned long)up, ts.c_str(), csvField(ssid).c_str(),
               WiFi.BSSIDstr(i).c_str(), rssi, (long)WiFi.channel(i), m ? 1 : 0);
    }
  }
  if (f) {
    if (n == 0) f.printf("%lu,%s,,,,,0\n", (unsigned long)up, ts.c_str());  // 0件も記録する
    f.close();
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  return r;
}

void scanlogDump(Stream& out) {
  for (const char* path : {kLogOldPath, kLogPath}) {
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    out.printf("# %s (%u bytes)\n", path, (unsigned)f.size());
    uint8_t buf[256];
    while (f.available()) {
      size_t len = f.read(buf, sizeof(buf));
      out.write(buf, len);
    }
    f.close();
  }
  out.println("# end");
}

void scanlogClear() {
  LittleFS.remove(kLogPath);
  LittleFS.remove(kLogOldPath);
}

size_t scanlogSize() {
  size_t total = 0;
  for (const char* path : {kLogOldPath, kLogPath}) {
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    total += f.size();
    f.close();
  }
  return total;
}
