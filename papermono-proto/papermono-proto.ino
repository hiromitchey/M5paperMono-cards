// PaperMono プロトタイプ:ローソン検知でPonta表示
// 設計: doc/proto-design.md
//
// ボード設定(Arduino IDE):
//   ボード: M5PaperMono(M5Stack 3.3.9 以上)
//   PSRAM: OPI PSRAM            ← 必須。無いと画面が初期化されない
//   Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
//     ※実際の配置はスケッチ内の partitions.csv が優先される(LittleFS用 spiffs 領域あり)

#include <LittleFS.h>
#include <M5Unified.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

#include "config.h"
#include "scanner.h"
#include "screens.h"

namespace {
enum class Mode { Profile, Lawson };

constexpr uint32_t kManualHoldMs = 5UL * 60 * 1000;  // 手動切り替えを保つ時間
constexpr uint32_t kBatteryLogMs = 10UL * 60 * 1000;  // 電池ログの間隔
constexpr const char* kBatteryLogPath = "/battery.csv";

struct LineWidth {
  int narrow, wide;
};
constexpr LineWidth kWidths[] = {{2, 5}, {3, 8}, {4, 9}};

Mode g_mode = Mode::Profile;
int g_hits = 0;
int g_misses = 0;
int g_last_rssi = 0;
int g_last_ap_count = -1;
uint32_t g_next_scan_ms = 0;
uint32_t g_manual_until_ms = 0;  // この時刻までは自動切り替えしない
bool g_manual_lawson = false;
uint32_t g_next_batt_log_ms = 0;
String g_last_scan_label;
String g_serial_line;

bool manualHoldActive() { return g_manual_until_ms != 0 && (int32_t)(millis() - g_manual_until_ms) < 0; }

String clockLabel() {
  auto dt = M5.Rtc.getDateTime();
  if (dt.date.year >= 2025) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", dt.time.hours, dt.time.minutes);
    return buf;
  }
  return "up " + String(millis() / 1000) + "s";
}

StatusInfo makeStatus() {
  StatusInfo st;
  st.battery = M5.Power.getBatteryLevel();
  st.last_scan = g_last_scan_label;
  st.lawson_hits = g_hits;
  st.ap_count = g_last_ap_count;
  return st;
}

void showProfile() {
  g_mode = Mode::Profile;
  g_manual_lawson = false;
  drawProfile(makeStatus());
}

void showLawson(bool manual) {
  g_mode = Mode::Lawson;
  g_manual_lawson = manual;
  LawsonInfo info;
  info.manual = manual;
  info.rssi = g_last_rssi;
  drawLawson(info);
}

void scheduleScan(uint32_t afterSec) { g_next_scan_ms = millis() + afterSec * 1000UL; }

void doScan() {
  const ScanResult r = scanOnce();
  g_last_scan_label = clockLabel();
  if (!r.ok) {
    Serial.println("[scan] 失敗");
    scheduleScan(g_config.scan.interval_sec);
    return;
  }
  g_last_ap_count = r.ap_count;
  const auto& sc = g_config.scan;

  if (r.matched) {
    g_misses = 0;
    ++g_hits;
    g_last_rssi = r.best_rssi;
    Serial.printf("[scan] 検知 %s %ddBm (%d/%d) AP=%d\n", r.best_ssid.c_str(), r.best_rssi, g_hits,
                  sc.enter_hits, r.ap_count);
  } else {
    g_hits = 0;
    ++g_misses;
    Serial.printf("[scan] 検知なし (%d/%d) AP=%d\n", g_misses, sc.exit_misses, r.ap_count);
  }

  if (!manualHoldActive()) {
    if (g_mode == Mode::Profile && g_hits >= sc.enter_hits) {
      showLawson(false);
    } else if (g_mode == Mode::Lawson && g_misses >= sc.exit_misses) {
      showProfile();
    }
  }

  // 1回目の検知なら確認スキャンを早める
  if (g_mode == Mode::Profile && g_hits > 0 && g_hits < sc.enter_hits) {
    scheduleScan(sc.confirm_after_sec);
  } else {
    scheduleScan(sc.interval_sec);
  }

  // バーコード表示中はステータスを更新しない
  if (g_mode == Mode::Profile) updateProfileStatus(makeStatus());
}

void logBattery() {
  const bool isNew = !LittleFS.exists(kBatteryLogPath);
  File f = LittleFS.open(kBatteryLogPath, "a");
  if (!f) return;
  if (isNew) f.println("uptime_s,time,level,mv,charging");
  auto dt = M5.Rtc.getDateTime();
  char ts[24] = "";
  if (dt.date.year >= 2025) {
    snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d", dt.date.year, dt.date.month, dt.date.date,
             dt.time.hours, dt.time.minutes, dt.time.seconds);
  }
  f.printf("%lu,%s,%d,%d,%d\n", (unsigned long)(millis() / 1000), ts, M5.Power.getBatteryLevel(),
           M5.Power.getBatteryVoltage(), (int)M5.Power.isCharging());
  f.close();
}

// 再起動の理由を /boot.csv に残す(持ち歩き中に頻繁に再起動していたため)
constexpr const char* kBootLogPath = "/boot.csv";

const char* resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "poweron";
    case ESP_RST_EXT: return "ext";
    case ESP_RST_SW: return "sw";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "int_wdt";
    case ESP_RST_TASK_WDT: return "task_wdt";
    case ESP_RST_WDT: return "wdt";
    case ESP_RST_DEEPSLEEP: return "deepsleep";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "sdio";
    case ESP_RST_USB: return "usb";
    case ESP_RST_JTAG: return "jtag";
    default: return "unknown";
  }
}

void logBoot() {
  const esp_reset_reason_t r = esp_reset_reason();
  Serial.printf("[boot] reset reason=%s(%d)\n", resetReasonName(r), (int)r);
  const bool isNew = !LittleFS.exists(kBootLogPath);
  File f = LittleFS.open(kBootLogPath, "a");
  if (!f) return;
  if (isNew) f.println("time,reason,code,battery_mv");
  auto dt = M5.Rtc.getDateTime();
  char ts[24] = "";
  if (dt.date.year >= 2025) {
    snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d", dt.date.year, dt.date.month, dt.date.date,
             dt.time.hours, dt.time.minutes, dt.time.seconds);
  }
  f.printf("%s,%s,%d,%d\n", ts, resetReasonName(r), (int)r, M5.Power.getBatteryVoltage());
  f.close();
}

void dumpFile(const char* path) {
  File f = LittleFS.open(path, "r");
  if (!f) {
    Serial.printf("(%s なし)\n", path);
    return;
  }
  while (f.available()) Serial.write(f.read());
  f.close();
  Serial.println("# end");
}

// "2026-11-23T12:34:56" または "2026-11-23 12:34:56"
bool setClock(const String& s) {
  int y, mo, d, h, mi, se;
  if (sscanf(s.c_str(), "%d-%d-%d%*c%d:%d:%d", &y, &mo, &d, &h, &mi, &se) != 6) return false;
  m5::rtc_datetime_t dt;
  dt.date.year = y;
  dt.date.month = mo;
  dt.date.date = d;
  dt.time.hours = h;
  dt.time.minutes = mi;
  dt.time.seconds = se;
  M5.Rtc.setDateTime(dt);
  return true;
}

void printHelp() {
  Serial.println(
      "コマンド:\n"
      "  help                 この一覧\n"
      "  status               状態を表示\n"
      "  scan                 すぐにスキャン\n"
      "  dump / clear         スキャンログ(CSV)の出力 / 消去\n"
      "  batt                 電池ログ(CSV)の出力\n"
      "  boots                再起動ログ(理由つき)の出力\n"
      "  config               config.json を表示\n"
      "  config {...}         1行のJSONで config.json を保存して再描画\n"
      "  time 2026-11-23T12:34:56   RTCの時刻を設定\n"
      "  lawson / profile     画面を手動で切り替え(5分保持)\n"
      "  sleep on|off         ライトスリープの切り替え(一時的)\n"
      "  format               LittleFSを初期化(設定・ログが消える)\n"
      "  reboot               再起動");
}

void printStatus() {
  Serial.printf("mode=%s hits=%d misses=%d manual_hold=%d rssi=%d ap=%d\n",
                g_mode == Mode::Lawson ? "LAWSON" : "PROFILE", g_hits, g_misses, (int)manualHoldActive(),
                g_last_rssi, g_last_ap_count);
  Serial.printf("battery=%d%% %dmV charging=%d vbus=%dmV usb=%d config=%s light_sleep=%d\n",
                M5.Power.getBatteryLevel(), M5.Power.getBatteryVoltage(), (int)M5.Power.isCharging(),
                M5.Power.getVBUSVoltage(), (int)Serial.isPlugged(),
                g_config.loaded_from_file ? "file" : "default",
                (int)g_config.power.light_sleep);
  Serial.printf("littlefs used=%u/%u scanlog=%u bytes\n", (unsigned)LittleFS.usedBytes(),
                (unsigned)LittleFS.totalBytes(), (unsigned)scanlogSize());
  Serial.printf("card=%s %s len=%u %dpx/%dpx\n", g_config.card.label.c_str(), g_config.card.format.c_str(),
                (unsigned)g_config.card.value.length(), g_config.card.narrow_px, g_config.card.wide_px);
}

void redrawCurrent() {
  if (g_mode == Mode::Lawson) showLawson(g_manual_lawson);
  else showProfile();
}

void handleCommand(String line) {
  line.trim();
  if (line.isEmpty()) return;
  String cmd = line;
  String arg;
  const int sp = line.indexOf(' ');
  if (sp > 0) {
    cmd = line.substring(0, sp);
    arg = line.substring(sp + 1);
    arg.trim();
  }

  if (cmd == "help") printHelp();
  else if (cmd == "status") printStatus();
  else if (cmd == "scan") doScan();
  else if (cmd == "dump") scanlogDump(Serial);
  else if (cmd == "clear") { scanlogClear(); Serial.println("スキャンログを消去"); }
  else if (cmd == "batt") dumpFile(kBatteryLogPath);
  else if (cmd == "boots") dumpFile(kBootLogPath);
  else if (cmd == "config") {
    if (arg.isEmpty()) {
      printConfig(Serial);
    } else {
      String err;
      if (saveConfigJson(arg, err)) {
        Serial.println("config.json を保存");
        redrawCurrent();
      } else {
        Serial.printf("保存できません: %s\n", err.c_str());
      }
    }
  } else if (cmd == "time") {
    if (setClock(arg)) Serial.println("RTCを設定");
    else Serial.println("形式: time 2026-11-23T12:34:56");
  } else if (cmd == "lawson") {
    g_manual_until_ms = millis() + kManualHoldMs;
    showLawson(true);
  } else if (cmd == "profile") {
    g_manual_until_ms = millis() + kManualHoldMs;
    showProfile();
  } else if (cmd == "sleep") {
    g_config.power.light_sleep = (arg == "on");
    Serial.printf("light_sleep=%d\n", (int)g_config.power.light_sleep);
  } else if (cmd == "format") {
    LittleFS.format();
    Serial.println("LittleFSを初期化");
  } else if (cmd == "reboot") {
    ESP.restart();
  } else {
    Serial.println("不明なコマンド。help で一覧");
  }
}

void pollSerial() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      handleCommand(g_serial_line);
      g_serial_line = "";
    } else if (g_serial_line.length() < 4096) {
      g_serial_line += c;
    }
  }
}

void cycleLineWidth() {
  auto& c = g_config.card;
  size_t idx = 0;
  for (size_t i = 0; i < sizeof(kWidths) / sizeof(kWidths[0]); ++i) {
    if (kWidths[i].narrow == c.narrow_px) idx = i;
  }
  idx = (idx + 1) % (sizeof(kWidths) / sizeof(kWidths[0]));
  c.narrow_px = kWidths[idx].narrow;
  c.wide_px = kWidths[idx].wide;
  Serial.printf("線幅 %dpx/%dpx\n", c.narrow_px, c.wide_px);
}

void handleButtons() {
  // KEY1(G2)
  if (M5.BtnA.wasHold()) {
    if (g_mode == Mode::Lawson) {
      cycleLineWidth();
      showLawson(g_manual_lawson);
    }
  } else if (M5.BtnA.wasClicked()) {
    g_manual_until_ms = millis() + kManualHoldMs;
    if (g_mode == Mode::Profile) showLawson(true);
    else showProfile();
  }
  // KEY2(G3)
  if (M5.BtnB.wasHold()) {
    fullRefresh();
    redrawCurrent();
  } else if (M5.BtnB.wasClicked()) {
    doScan();
  }
}

// ライトスリープ中はUSBが止まり、PCからCOMポートを開けなくなる(書き込みもできない)。
// なので USB給電中・シリアル接続中・起動直後は眠らない
constexpr uint32_t kNoSleepAfterBootMs = 60UL * 1000;

// isPlugged() は USB の SOF を見るので、PC側でポートを開いていなくても「つながっている」と分かる。
// (VBUS電圧はPaperMonoでは取れず、これだけでは眠ってしまった)
bool usbHostConnected() { return Serial.isPlugged() || M5.Power.getVBUSVoltage() > 4000; }

// 起きた直後はUSBの再接続(PC側の認識)が終わっておらず isPlugged() が false になる。
// そのまま眠ると「挿しても眠り続けてCOMポートが出ない」ので、起きてから少し待つ
constexpr uint32_t kAwakeAfterWakeMs = 3000;
uint32_t g_woke_at_ms = 0;

bool sleepAllowed() {
  if (!g_config.power.light_sleep) return false;
  if (millis() < kNoSleepAfterBootMs) return false;
  if (millis() - g_woke_at_ms < kAwakeAfterWakeMs) return false;
  if (usbHostConnected()) return false;
  return true;
}

// 次のスキャンまでライトスリープ。ボタン(G2/G3 Low)でも起きる
void idle() {
  const int32_t wait = (int32_t)(g_next_scan_ms - millis());
  const bool anyPressed = M5.BtnA.isPressed() || M5.BtnB.isPressed();
  if (!sleepAllowed() || wait < 1000 || anyPressed) {
    delay(10);
    return;
  }
  M5.Display.waitDisplay();
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)wait * 1000ULL);
  gpio_wakeup_enable(GPIO_NUM_2, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable(GPIO_NUM_3, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_light_sleep_start();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  g_woke_at_ms = millis();
}
}  // namespace

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.setRxBufferSize(4096);  // config の1行(約1KB)が画面描画中に溢れないように
  Serial.begin(115200);
  M5.BtnA.setHoldThresh(800);
  M5.BtnB.setHoldThresh(800);

  if (!LittleFS.begin(true)) {
    Serial.println("[fs] LittleFSをマウントできません(パーティション設定を確認)");
  }
  logBoot();
  if (loadConfig()) Serial.println("[config] config.json を読み込み");
  else Serial.println("[config] config.json なし。既定値で動作(番号はダミー)");

  scannerBegin();
  screensBegin();
  showProfile();
  printHelp();

  g_next_scan_ms = millis() + 2000;  // 起動直後に1回スキャン
  g_next_batt_log_ms = millis();
}

void loop() {
  M5.update();
  pollSerial();
  handleButtons();

  if ((int32_t)(millis() - g_next_scan_ms) >= 0) doScan();
  if ((int32_t)(millis() - g_next_batt_log_ms) >= 0) {
    logBattery();
    g_next_batt_log_ms = millis() + kBatteryLogMs;
  }
  idle();
}
