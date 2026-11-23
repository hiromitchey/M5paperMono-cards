#!/usr/bin/env python3
"""スキャンログ(dumpの出力)を読み、スキャンごとにローソンAPの最強RSSIを時系列で出す。

  python3 tools/analyze_scanlog.py logs/scanlog-20261123.csv
"""
import csv
import sys
from collections import OrderedDict

path = sys.argv[1]
scans = OrderedDict()  # (uptime, time) -> dict
rows = []
with open(path, encoding="utf-8", errors="replace") as f:
    lines = [l for l in f if l.strip() and not l.startswith("#") and not l.startswith("[")]
for r in csv.reader(lines):
    if len(r) < 7 or r[0] == "uptime_s":
        continue
    key = (r[0], r[1])
    s = scans.setdefault(key, {"ap": 0, "lawson": [], "matched": False})
    if r[2] or r[3]:
        s["ap"] += 1
    if "LAWSON" in r[2].upper() or "PM_TEST" in r[2]:
        s["lawson"].append((int(r[4]), r[2], r[3]))
    if r[6] == "1":
        s["matched"] = True

print(f"スキャン回数: {len(scans)}")
print("uptime  時刻                 AP  ローソンAP(最強RSSI)  判定")
prev = None
for (up, ts), s in scans.items():
    law = max(s["lawson"]) if s["lawson"] else None
    law_s = f"{law[0]:4d}dBm x{len(s['lawson'])}" if law else "-"
    print(f"{up:>6}  {ts or '(未設定)':19}  {s['ap']:2d}  {law_s:20}  {'○' if s['matched'] else ''}")

bssids = {}
for s in scans.values():
    for rssi, ssid, bssid in s["lawson"]:
        bssids.setdefault((ssid, bssid), []).append(rssi)
if bssids:
    print("\nローソン系AP:")
    for (ssid, bssid), v in bssids.items():
        print(f"  {ssid} {bssid}  {len(v)}回  最強{max(v)} 最弱{min(v)}")
