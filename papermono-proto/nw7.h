// NW-7(Codabar)エンコーダ。設計書 §5 の符号表どおり
#pragma once
#include <cstdint>
#include <cstring>

namespace nw7 {
constexpr const char* kAlphabet = "0123456789-$:/.+ABCD";
constexpr uint8_t kCodes[20] = {
  0x03, 0x06, 0x09, 0x60, 0x12, 0x42, 0x21, 0x24, 0x30, 0x48,  // 0-9
  0x0C, 0x18, 0x45, 0x51, 0x54, 0x15,                          // - $ : / . +
  0x1A, 0x29, 0x0B, 0x0E                                       // A B C D
};

// バーの位置(x)と幅(w)をコールバックに渡す。戻り値はバー部分の総幅。不正な文字なら -1
template <typename BarFn>
int encode(const char* s, int narrow, int wide, BarFn bar) {
  int x = 0;
  for (size_t i = 0; s[i] != '\0'; ++i) {
    const char* p = std::strchr(kAlphabet, s[i]);
    if (p == nullptr || *p == '\0') return -1;
    const uint8_t code = kCodes[p - kAlphabet];
    for (int k = 0; k < 7; ++k) {
      const int w = ((code >> (6 - k)) & 1) ? wide : narrow;
      if (k % 2 == 0) bar(x, w);          // 偶数番目がバー
      x += w;
    }
    if (s[i + 1] != '\0') x += narrow;    // 文字間のすき間
  }
  return x;
}

// 幅だけ計算する
inline int measure(const char* s, int narrow, int wide) {
  return encode(s, narrow, wide, [](int, int) {});
}
}  // namespace nw7
