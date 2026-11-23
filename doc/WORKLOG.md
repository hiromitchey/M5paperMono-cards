# WORKLOG

## 2026-11-23

- プロト `papermono-proto/` を Arduino IDE 用に作成(スキャン・判定・画面・NW-7・設定・シリアルコマンド)。arduino-cli でコンパイル確認、警告なし
- 分かったこと:PaperMono は OPI PSRAM 必須。BtnA=G2 / BtnB=G3。IDE に LittleFS アップロード無し。シンボリックリンクは管理者権限が必要
- 決めたこと:Arduino IDE 採用、設定はシリアル送信、`tools/sync.sh` で同期。proto-design.md §7・§8・§11 を更新
- PLAN.md / WORKLOG.md を作成
- 次:実機に書き込んで起動確認、config.json を送る
- 実機に表示したPontaバーコードがスマホのリーダーで読めた(ユーザー確認)。次はレジでの確認
- `data/config.json` を作成(番号はユーザーが自分で入れる)
- Chrome経由でXからアイコン・表示名・紹介文を取得。WSLにPillow/pipが無く、get-pip.pyの導入は自動承認で拒否されたため、System.Drawing版の `tools/make_icon.ps1` を作成して `icon_png.h` を生成。コンパイル確認済み
- 次:アイコン入りを書き込み、config.json(番号入り)を送ってレジで確認
- Arduino IDE が「移動した」と表示。rsync の一時ファイル→改名が原因と推定し、sync.sh に `--inplace` を追加(未確認)
- 書き込みで COM3 が開けない(PermissionError 31)。原因はライトスリープでUSBが止まること。USB給電中・起動後60秒は眠らないよう修正、コンパイル確認済み。今回はダウンロードモード(電源ボタン長押し→赤LED点滅)で書き込む
- ダウンロードモードで書き込み成功(ユーザー確認)。次回以降に通常の書き込みが通るかは未確認
- config をシリアルで送ろうとしたら、ポートを開いた時に本体が再起動して1行が切れた。さらに起動60秒後に眠ってCOM3が消えた → VBUS電圧はPaperMonoでは取れない。`Serial.isPlugged()`(SOF検知)に切り替え、受信バッファを4KBに。コンパイル確認済み、未書き込み
- 次:ユーザーが config.json に番号を入れる → ダウンロードモードで書き込み → config を送る
- ユーザー提供の写真(assets/、gitignore済み)から番号を読み取り config.json に設定。本体は眠っていてCOMポートなし。書き込み待ち
- 修正版の書き込み後、シリアルで config を送信し保存成功(config=file)。`usb=1` で isPlugged は有効。ただし電池が `battery=-2% 0mV vbus=-1mV` で取れていない → 要調査
- 実機の画面が自分のプロフィール・番号に変わったことをユーザーが確認
- 次:LAWSON画面の番号をスマホで照合 → レジで確認。電池残量の取得を調査
- 実店舗テスト成功(ユーザー報告):Wi-Fiで自動でLAWSON画面に切り替わり、レジでPontaが読めた。Phase0クリア。policy.md の要確認2件をチェック
- 次:スキャンログ回収(検知距離・RSSI)、店を出た後の復帰確認、電池残量の取得
- 帰宅後、本体が応答せず。COM3が出たり消えたり → 眠りから起きた直後は isPlugged() が false で再び眠るループ。起床後3秒は眠らないよう修正
- 赤LED点滅=ダウンロードモードで止まっていた。arduino-cli で書き込み後もブートローダーのままだったので、esptool `--after watchdog-reset` で起動させた。DTR=1 でポートを開くと本体がリセットされるので、以後は DTR=0 で開く
- スキャンログ分析:LAWSON_Free_Wi-Fi(1台)を -65→-53dBm で検知、2回目で切り替え。直前のスキャン(34秒前)では見えておらず、間隔が長かったのが遅れの原因 → 15秒/3秒に変更(config・既定値とも)
- 分かったこと:uptime が数分おきに 15〜25s に戻っており、**持ち歩き中に頻繁に再起動**している。店内でも再起動してPROFILEに戻っていた。原因不明 → `/boot.csv` に再起動理由を記録する機能を追加して書き込み済み(シリアル `boots`)
- 電池は取れていた(86%)。-2% は起動直後の1回だけ
- 次:もう一度持ち歩いて `boots` で再起動理由を確認、15秒間隔で入店時に間に合うか確認
- git init・初回コミット、README 追加。GitHub(Public、ユーザーが作成)へ push 予定。config.json・番号写真・logs/ は除外を確認
