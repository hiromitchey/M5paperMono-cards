# PaperMono ポイントカード財布 方針

更新: 2026-11-23

## 目的

- 財布のポイントカードと、スマホの容量を食う店の公式アプリを、PaperMono 1台に置き換える
- 店の近くで候補を自動表示し、タップでバーコードを出す
- なくした時に探せるようにする

## 前提(ハード)

- 端末:M5Stack PaperMono(NFC・LoRa付き)。技適マークあり
- SoC:ESP32-S3R8(Wi-Fi 2.4GHzのみ、BLE)
- 画面:3.97インチ 480×800 モノクロ電子ペーパー(4階調)、タッチ、フロントライト。約235ppiなので、線の最小単位3pxで約0.32mm
- その他:microSD、IMU(BMI270)、RTC、ブザー、RGB LED、1150mAh
- **GPSも拡張ポートもない** → 位置はWi-Fiとスマホから推定する
- 電源は段階式(L0〜L3B)。IMUの動き検知やRTCでスリープから起こす機能は、公式に対応(L1を保持したまま眠らせる)
- 充電:USB-C、充電ICはIP2315。充電時間・電流・LED表示は公式に記載がないので実測する。残量は `M5.Power.getBatteryLevel()` で取れる
  - 注意:電池電圧が低いと充電ICがI2Cバスに干渉することがある。早めに自分で電源を落とす

## カード・店ごとの扱い

| 対象 | バーコード | 番号 | 方針 |
| --- | --- | --- | --- |
| ローソン(Ponta) | NW-7(A+15桁+A) | 固定 | PaperMonoで表示。アプリは消してOK(クーポンは失う) |
| ファミマ | ファミペイはCODE128・26桁 | 数分で変わる | ファミペイは移せない。Vポイントの固定番号を登録する(ランク特典・クーポンは失う) |
| セブン | CODE128・18桁 | 固定 | 画面に「複製は無効」の記載あり。自己判断。還元0.5%なので諦めるのも手 |
| その他の物理カード | バーコード/QR | 固定が多い | PaperMonoへ |
| 対象外 | 磁気のみ、ICチップ型、変わるコード、クレジットカード | ― | 扱わない |

- アプリを消して一番失うのはクーポン。よく使う店は、Androidの「アーカイブ」(機種による)でデータを残したまま本体だけ消す手もある

## 機能の方針

| 優先度 | 機能 |
| --- | --- |
| 必須 | カード一覧、バーコード表示(JAN/NW-7/CODE128/CODE39/ITF/QR)、表示の保持、本体の登録ページ(スマホ撮影→ZXing-js、手入力)、Wi-Fiでの店候補、上位3件表示 |
| あると便利 | 行動パターン加点、IMUでの取り出し検知、Androidアプリ(ジオフェンス→BLE、置き忘れ通知、鳴らして探す)、拾った人向け表示 |
| お試し | 先回り表示、Google Find Hub(GoogleFindMyTools)、交通系IC残高 |
| やらない | クレジットカード、LoRa、公式アプリの自動操作や通信解析 |

## 位置推定

- 基本はWi-Fi
  1. SSID名で判定(例:LAWSON_Free_Wi-Fi)
  2. カードを使った時に周囲のBSSIDを記録して学習
- ESP32-S3は2.4GHzしか見えないので、`_5G` が付いたSSIDは使えない
- セブン(7SPOT)とファミマ(Famima_Wi-Fi)の無料Wi-Fiは2022年に終了済み
- スマホ連携(Android)は後回し

## 紛失対策

- 置き忘れ通知(BLEが切れたらスマホに通知し、切れた地点を記録)
- 鳴らして探す(ブザー・LED・フロントライト)
- 拾った人向けの待ち受け表示(連絡先は最低限)
- Find Hubはお試し枠。非公式品・MACアドレス固定・3日ごとの再登録問題(要確認)あり

## 電池

- 待機中はL1(IMU)だけ残して深く眠らせ、持ち上げたら起きる
- 重い処理はWi-Fiスキャンと、Find HubのBLE常時発信
- Find Hubは「外出中だけオン」など割り切る前提

## 進め方

1. **プロトタイプ**:ローソン検知→Pontaバーコード、それ以外はプロフィール+QR(設計は別紙 `proto-design.md`)。レジで読めるかの確認(Phase0)も兼ねる
2. カード一覧・登録ページ
3. 店候補(BSSID学習、上位3件)
4. IMU起動・拾った人向け表示
5. Androidアプリ連携
6. Find Hub

## 要確認(残り)

- [x] 技適マーク
- [x] IMUでスリープから起こせるか(公式対応)
- [x] 電子ペーパーのバーコードがレジで読めるか → 読めた(2026-11-23、レジ種別は未記録)
- [ ] M5GFXの描画設定が不安定な件(公式のOTP波形サンプルとの比較)
- [ ] Wi-Fiスキャン・BLE発信での電池の持ち
- [ ] microSDの電源投入(M5IOE1経由)
- [ ] Find Hubの「3日ごとに再登録」問題の現状
- [x] 近所のローソンでLAWSON_Free_Wi-Fiが2.4GHzで見えるか → 見えた(自動切り替え成功)

## 参考

- 詳細版ドキュメント:[PaperMono ポイントカード財布 機能まとめ](https://claude.ai/artifact/LokBAQKMQjF4dGtrcpWWc7)
- [PaperMono 公式ドキュメント](https://docs.m5stack.com/ja/core/PaperMono)
- [PaperMono 電源管理(M5PM1/M5IOE1)](https://docs.m5stack.com/en/arduino/papermono/m5pm1_m5ioe1)
- [PaperMono バッテリーAPI](https://docs.m5stack.com/ja/arduino/papermono/battery)
- [GoogleFindMyTools](https://github.com/leonboe1/GoogleFindMyTools)