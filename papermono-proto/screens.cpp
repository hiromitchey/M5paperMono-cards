#include "screens.h"

#include <LittleFS.h>
#include <M5Unified.h>

#include "config.h"
#include "nw7.h"

// アイコンをスケッチに埋め込む場合は tools/make_icon.py で icon_png.h を作る
#if __has_include("icon_png.h")
#include "icon_png.h"
#define HAS_EMBEDDED_ICON 1
#endif

namespace {
enum class Screen { None, Profile, Lawson };
Screen g_current = Screen::None;
int g_partial_count = 0;
StatusInfo g_last_status;

constexpr int kStatusY = 784;
constexpr int kStatusH = 16;

// 縦長(幅<高さ)になる回転を選ぶ。パネルの向き設定に依存しないようにする
void setPortrait() {
  for (int r = 0; r < 4; ++r) {
    M5.Display.setRotation(r);
    if (M5.Display.width() < M5.Display.height()) return;
  }
}

void setLandscape() {
  for (int r = 0; r < 4; ++r) {
    M5.Display.setRotation(r);
    if (M5.Display.width() > M5.Display.height()) return;
  }
}

void beginFull() {
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
}

void endAndShow() {
  M5.Display.endWrite();
  M5.Display.display();
  M5.Display.waitDisplay();
}

void drawCentered(const String& s, int y, const lgfx::IFont* font) {
  M5.Display.setFont(font);
  M5.Display.setTextDatum(textdatum_t::top_center);
  M5.Display.drawString(s, M5.Display.width() / 2, y);
}

// UTF-8を1文字ずつ取り出す
String nextUtf8Char(const String& s, size_t& i) {
  const uint8_t c = s[i];
  size_t len = 1;
  if (c >= 0xF0) len = 4;
  else if (c >= 0xE0) len = 3;
  else if (c >= 0xC0) len = 2;
  String ch = s.substring(i, i + len);
  i += len;
  return ch;
}

// 文字単位で折り返して描く。改行コードも尊重する
void drawWrapped(const String& text, int x, int y, int width, int maxLines, const lgfx::IFont* font) {
  M5.Display.setFont(font);
  M5.Display.setTextDatum(textdatum_t::top_left);
  const int lh = M5.Display.fontHeight() + 4;
  String line;
  int lines = 0;
  size_t i = 0;
  auto flush = [&]() {
    if (lines < maxLines) M5.Display.drawString(line, x, y + lines * lh);
    ++lines;
    line = "";
  };
  while (i < text.length() && lines < maxLines) {
    String ch = nextUtf8Char(text, i);
    if (ch == "\n") {
      flush();
      continue;
    }
    if (M5.Display.textWidth(line + ch) > width) {
      // 最終行に収まらなければ「…」で切る
      if (lines == maxLines - 1) {
        while (line.length() > 0 && M5.Display.textWidth(line + "…") > width) {
          // 末尾の1文字(UTF-8)を落とす
          int k = line.length() - 1;
          while (k > 0 && (static_cast<uint8_t>(line[k]) & 0xC0) == 0x80) --k;
          line.remove(k);
        }
        line += "…";
        flush();
        return;
      }
      flush();
    }
    line += ch;
  }
  if (line.length() > 0 && lines < maxLines) flush();
}

void drawIcon(int x, int y, int size) {
  const String& path = g_config.profile.icon;
  if (path.length() > 0 && LittleFS.exists(path)) {
    M5.Display.drawPngFile(LittleFS, path.c_str(), x, y, size, size);
    return;
  }
#ifdef HAS_EMBEDDED_ICON
  M5.Display.drawPng(kIconPng, sizeof(kIconPng), x, y, size, size);
#else
  // 仮のアイコン:円に頭文字
  M5.Display.fillCircle(x + size / 2, y + size / 2, size / 2 - 2, TFT_LIGHTGREY);
  M5.Display.drawCircle(x + size / 2, y + size / 2, size / 2 - 2, TFT_BLACK);
  String h = g_config.profile.handle;
  String initial = h.startsWith("@") && h.length() > 1 ? h.substring(1, 2) : "?";
  initial.toUpperCase();
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(textdatum_t::middle_center);
  M5.Display.setTextColor(TFT_BLACK);
  M5.Display.drawString(initial, x + size / 2, y + size / 2);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
#endif
}

String statusText(const StatusInfo& st) {
  String s = "電池 ";
  s += st.battery >= 0 ? String(st.battery) + "%" : String("--");
  s += "  最終スキャン ";
  s += st.last_scan.length() ? st.last_scan : String("--");
  if (st.ap_count >= 0) s += "  AP " + String(st.ap_count);
  if (st.lawson_hits > 0) s += "  検知 " + String(st.lawson_hits);
  if (!g_config.loaded_from_file) s += "  [既定値]";
  return s;
}

void drawStatusLine(const StatusInfo& st) {
  M5.Display.fillRect(0, kStatusY, M5.Display.width(), kStatusH, TFT_WHITE);
  M5.Display.setFont(&fonts::lgfxJapanGothic_12);
  M5.Display.setTextDatum(textdatum_t::top_left);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(statusText(st), 8, kStatusY + 2);
}

// "A123456789012345A" → "1234 5678 9012 345"
String humanNumber(const String& v) {
  String body = v;
  if (body.length() >= 2 && strchr("ABCD", body[0]) && strchr("ABCD", body[body.length() - 1])) {
    body = body.substring(1, body.length() - 1);
  }
  String out;
  for (size_t i = 0; i < body.length(); ++i) {
    if (i > 0 && i % 4 == 0) out += ' ';
    out += body[i];
  }
  return out;
}
}  // namespace

void screensBegin() {
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.setColorDepth(8);
}

void drawProfile(const StatusInfo& st) {
  setPortrait();
  beginFull();
  const auto& p = g_config.profile;

  drawIcon(120, 32, 240);
  drawCentered(p.name, 296, &fonts::lgfxJapanGothic_32);
  drawCentered(p.handle, 340, &fonts::lgfxJapanGothic_24);
  drawWrapped(p.bio, 32, 380, 416, 4, &fonts::lgfxJapanGothic_20);

  M5.Display.qrcode(p.url.c_str(), 124, 508, 232, 3);

  String shortUrl = p.url;
  shortUrl.replace("https://", "");
  shortUrl.replace("http://", "");
  drawCentered(shortUrl, 752, &fonts::lgfxJapanGothic_20);

  drawStatusLine(st);
  endAndShow();

  g_current = Screen::Profile;
  g_partial_count = 0;
  g_last_status = st;
}

void updateProfileStatus(const StatusInfo& st) {
  if (g_current != Screen::Profile) return;
  if (statusText(st) == statusText(g_last_status)) return;  // 変化なしなら描かない
  if (++g_partial_count >= 10) {
    drawProfile(st);  // 10回ごとに全面描き換え
    return;
  }
  M5.Display.setEpdMode(epd_mode_t::epd_fastest);
  M5.Display.startWrite();
  drawStatusLine(st);
  endAndShow();
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  g_last_status = st;
}

void drawLawson(const LawsonInfo& info) {
  setLandscape();
  beginFull();
  const auto& c = g_config.card;
  const int W = M5.Display.width();

  M5.Display.setFont(&fonts::lgfxJapanGothic_24);
  M5.Display.setTextDatum(textdatum_t::top_left);
  M5.Display.drawString("LAWSON / " + c.label, 24, 24);

  const int width = nw7::measure(c.value.c_str(), c.narrow_px, c.wide_px);
  const int quiet = c.narrow_px * 10;
  if (width < 0) {
    drawCentered("バーコードの値に使えない文字があります", 200, &fonts::lgfxJapanGothic_24);
  } else if (width + quiet * 2 > W) {
    drawCentered("線幅 " + String(c.narrow_px) + "px では画面に収まりません", 200,
                 &fonts::lgfxJapanGothic_24);
  } else {
    const int x0 = (W - width) / 2;
    nw7::encode(c.value.c_str(), c.narrow_px, c.wide_px,
                [&](int x, int bw) { M5.Display.fillRect(x0 + x, 120, bw, 220, TFT_BLACK); });
  }

  drawCentered(humanNumber(c.value), 372, &fonts::lgfxJapanGothic_32);

  String info_s = String(c.narrow_px) + "px/" + String(c.wide_px) + "px";
  if (info.manual) info_s += " / 手動";
  else if (info.rssi != 0) info_s += " / " + String(info.rssi) + "dBm";
  M5.Display.setFont(&fonts::lgfxJapanGothic_16);
  M5.Display.setTextDatum(textdatum_t::bottom_right);
  M5.Display.drawString(info_s, W - 16, M5.Display.height() - 12);

  endAndShow();
  g_current = Screen::Lawson;
}

void fullRefresh() {
  M5.Display.setEpdMode(epd_mode_t::epd_quality);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.display();
  M5.Display.waitDisplay();
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.display();
  M5.Display.waitDisplay();
}
