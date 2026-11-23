# papermono-cards

M5Stack PaperMono(電子ペーパー)を、ポイントカード財布にする試み。

いまはプロトタイプ段階:**ローソンのWi-Fiを検知したらPontaのバーコードを出し、それ以外はXのプロフィールとQRを出す。**
実店舗で、自動切り替えとレジでの読み取りを確認済み(2026-11-23)。

## 構成

| パス | 内容 |
| --- | --- |
| `papermono-proto/` | Arduino IDE 用スケッチ |
| `tools/` | Arduinoフォルダとの同期、アイコン変換、設定送信、スキャンログ分析 |
| `doc/` | 方針(`policy.md`)、設計(`proto-design.md`)、`PLAN.md`、`WORKLOG.md` |

## 使い方(概要)

1. Arduino IDE で `papermono-proto` を開く。ボード「M5PaperMono」、**PSRAM: OPI PSRAM**、Partition Scheme「16M Flash (3MB APP/9.9MB FATFS)」
2. 書き込む
3. `papermono-proto/data/config.example.json` を `config.json` にコピーし、カード番号などを入れる
4. `python3 tools/config_oneline.py` の出力をシリアルモニタ(115200)に貼って送る

詳しくは `doc/proto-design.md`。

カード番号は `config.json` にだけ置き、リポジトリには入れない(`.gitignore` 済み)。
