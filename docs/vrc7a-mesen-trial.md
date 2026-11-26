# VRC7a 移植版・診断用 ROM を Mesen で試す

ZIP を展開する。Mesen 2.1.1 の NES 設定を NTSC にし、1P に NES コントローラーを割り当てる。スプライト制限を有効にし、オーバースキャンを上下左右とも0にして設定を保存する。

本編を遊ぶときは `build/vrc7a-test-512k.nes` を、試作7面・基盤診断を試すときは `build/vrc7a-test-512k-prototype.nes`（または 32 KiB 診断版 `build/vrc7a-test.nes`）を Mesen で開く。

## 操作とゲームモード

### 1. 本編（アーケード Revision G 準拠・全7区間＋2周目：`build/vrc7a-test-512k.nes`）

起動するとタイトル画面（`1P START` / `OPTION`）が表示される。

- **上下＋START / A**: `1P START` でオープニング演出を経て Stage 1 を開始（**右＋START** で演出スキップ）。`OPTION` でオプション画面（`INVINCIBLE` 無敵モード / `STAGE 1..7` / `LOOP 1..2` / `WEAPON` / `TIME LIMIT` / `LIVES` / `SOUND TEST`）を開く。
- **SELECT**: VRC7a 診断メニュー（`VRC7A TEST MENU`）へ切り替える。
- **移動・アクション**: 十字キー左右で歩行、下でしゃがみ（しゃがみ撃ち可）、上下ではしご昇降、**A** でジャンプ（空中制御固定）、**B** で射撃、**START** でポーズ。
- **ステージ進行と 2 周目**: Stage 1（墓場〜森〜一角獣）→ Stage 2（魔物の村〜氷の館〜一角獣×2）→ Stage 3（洞窟）→ Stage 4（浮遊足場〜ドラゴン）→ Stage 5（魔城下層〜サタン）→ Stage 6（魔城上層〜サタン×2・十字架未所持時は Stage 5 へ差し戻し）→ Stage 7（玉座〜アスタロト）。1周目クリアで 2 周目へ突入し、2周目 Stage 7 撃破で `FINAL CLEAR`。

### 2. 試作7面アーケードモード（`build/vrc7a-test-512k-prototype.nes`）

起動するとタイトル画面（`MAKAIMURA FC / VRC7A EDITION`・タイトル BGM 再生）が表示される。

- **START**: VRC7a FM 6声＋APU 音源と全7区間＋2周目のアーケードモードを開始する（従来の **右＋START** でも開始可能）。
- **SELECT**: VRC7a 診断メニュー（`VRC7A TEST MENU`）へ切り替える。
- **移動・アクション**: 十字キー左右で歩行、下でしゃがみ（しゃがみ撃ち可）、上下ではしご昇降、**A** でジャンプ（空中制御固定）、**B** で射撃（地上 15F 硬直）。
- **鎧と残機**: 初期状態は鎧あり（2段階ライフ：被弾で裸＋ノックバック＋120F 無敵、再被弾または穴落下・時間切れで1機失う）。初期残機 3、制限時間 120 秒。スコア初回 2,000 点・以降 +5,000 点ごとに 1UP（上限 9）。
- **HUD**: 上段 `STG # LIFE # TIME ### CP # W`（`W` = 武器 `L` 槍 / `D` 短剣 / `T` 松明 / `S` 聖盾）、下段 `SCORE ######    LP #`（周回数 `LP 1` / `LP 2`）。
- **ステージ進行と必須武器分岐**: Stage 1〜7 をクリアして進む。Stage 6 で必須武器「聖盾」（`S`）を未所持のままボスを倒すと Stage 5 へ差し戻される。1周目 Stage 7 撃破後は `COURSE CLEAR`（`START CONT`）から2周目 Stage 1（`LP 2`・難易度ランク維持）へ進み、2周目 Stage 7 撃破で真エンディング（`FINAL CLEAR` → **START** でタイトル復帰）となる。
- **リトライ・終了**: ミス後は **START** でチェックポイント（または開始地点）から再開。残機 0 で `GAME OVER` → **START** でタイトル復帰。プレイ中・クリア画面の **SELECT** でもタイトルに戻る。

### 3. VRC7a 診断メニュー・既存2コース（32 KiB 版 / 512 KiB 版の SELECT）

- タイトル画面で **SELECT** を押すと `VRC7A TEST MENU`（FM / APU / BOTH / ミュート切替、地形スクロール、2軸カメラ）へ移る。
- **B** で元のコース、**下＋B** で追加コースを開始する。ひし形の標識で途中復帰地点が有効になり、右端の旗に着地でクリア。
- 512 KiB 版で **上＋START** を押すと残機 3・制限時間 120 秒の2コース連続プレイを開始する（Stage 1 クリア画面 `START STG2` から Stage 2 へ進行）。

## 自動試験

展開先のルートで PowerShell を開き、Mesen.exe の場所を指定して実行する。

```powershell
./tools/test-mesen.ps1 -UseBuiltRoms -MesenPath 'C:/path/to/Mesen.exe'
```

同梱の 32 KiB 版では診断・既存系 14 スイート、512 KiB 試作版ではアーケード・コース系 13 スイート、512 KiB 本編では Revision G 全7区間＋オプション系 6 スイート（計 33 スイート）を実行する。本編のみ回す場合は `-Stage1` を付ける。結果は端末と `build/mesen-smoke/` に出力される。全 ROM の SHA-256 は `SHA256SUMS.txt` に記載している。
