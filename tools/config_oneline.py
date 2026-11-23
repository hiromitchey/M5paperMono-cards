#!/usr/bin/env python3
"""config.json を1行にして `config {...}` の形で出力する。

シリアルモニタに貼り付けて送ると、本体の LittleFS に保存される。
  python3 tools/config_oneline.py [papermono-proto/data/config.json]
"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "papermono-proto/data/config.json"
cfg = json.loads(path.read_text(encoding="utf-8"))

value = cfg.get("card", {}).get("value", "")
if value and not (value[0] in "ABCD" and value[-1] in "ABCD"):
    print("注意: card.value の前後に A などのスタート/ストップ文字がありません", file=sys.stderr)

line = "config " + json.dumps(cfg, ensure_ascii=False, separators=(",", ":"))
if len(line.encode("utf-8")) > 4000:
    sys.exit("長すぎます(4000バイト以内にしてください)")
print(line)
