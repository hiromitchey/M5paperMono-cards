#!/usr/bin/env bash
# WSL側の papermono-proto/ と、Windows側 Arduino スケッチブックのフォルダを同期する
#
#   tools/sync.sh          双方向(新しい方が勝つ)。削除は伝えない
#   tools/sync.sh push     WSL → Arduino のみ
#   tools/sync.sh pull     Arduino → WSL のみ
#   tools/sync.sh watch    2秒ごとに双方向同期(Ctrl+C で終了)
#
# Arduino側の場所は ARDUINO_SKETCH で変えられる
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/papermono-proto/"
DST="${ARDUINO_SKETCH:-/mnt/c/Users/negik/OneDrive/ドキュメント/Arduino/papermono-proto}/"

# build/ は Arduino IDE の「コンパイル済みバイナリを出力」で作られる
# --inplace: 一時ファイル→改名をしない(Arduino IDE が「ファイルが移動/削除された」と誤検知するため)
RSYNC=(rsync -rtu --inplace --itemize-changes --exclude 'build/' --exclude '.DS_Store' --exclude '*~')
# /mnt/c 上はパーミッションが当てにならないので -p は付けない。時刻のずれ対策で --modify-window

push() { mkdir -p "$DST"; "${RSYNC[@]}" --modify-window=2 "$SRC" "$DST"; }
pull() { [ -d "$DST" ] || return 0; "${RSYNC[@]}" --modify-window=2 "$DST" "$SRC"; }

case "${1:-both}" in
  push) push ;;
  pull) pull ;;
  both) pull; push ;;
  watch)
    echo "同期中: $SRC <-> $DST (Ctrl+C で終了)"
    while true; do
      out="$(pull; push)"
      [ -n "$out" ] && { date '+%H:%M:%S'; echo "$out"; }
      sleep 2
    done
    ;;
  *) echo "使い方: $0 [push|pull|both|watch]" >&2; exit 1 ;;
esac
