# 国内版 Revision G の 6809 ROM マップ

対象は MAME 0.289 の `makaimurg`。アドレスは CPU に見える位置を表し、切り替え窓の参照には `bank3:$553A` のようにバンクを付ける。

## ROM の配置

| 領域 | CPU アドレス | MAME の `maincpu` 内オフセット | ROM 内の範囲 |
| --- | --- | --- | --- |
| fixed | `$6000–$7FFF` | `$06000–$07FFF` | `mj04g.bin` の後半 8 KiB |
| fixed | `$8000–$FFFF` | `$08000–$0FFFF` | `mj03g.bin` 全体 |
| bank0 | `$4000–$5FFF` | `$10000–$11FFF` | `mj05g.bin` の `$0000–$1FFF` |
| bank1 | `$4000–$5FFF` | `$12000–$13FFF` | `mj05g.bin` の `$2000–$3FFF` |
| bank2 | `$4000–$5FFF` | `$14000–$15FFF` | `mj05g.bin` の `$4000–$5FFF` |
| bank3 | `$4000–$5FFF` | `$16000–$17FFF` | `mj05g.bin` の `$6000–$7FFF` |
| bank4 | `$4000–$5FFF` | `$04000–$05FFF` | `mj04g.bin` の前半 8 KiB |

`$3E00` への書き込み値が4なら bank4、それ以外は下位2ビットで bank0〜3を選ぶ。上の配置は MAME のメモリマップとローカルの `-listxml makaimurg` で照合した。さらに起動時の自己診断と同じ方法で計算した16ビット加算チェックサム8個が、`bank3:$5FF0–$5FFF` の期待値とすべて一致した。順序は bank4、3、2、1、0、固定 ROM の `$6000–$7FFF`、`$8000–$BFFF`、`$C000–$FFFF`。bank3 の加算範囲は `$4000–$5FEF`。[MAME のバンク選択](https://github.com/mamedev/mame/blob/mame0289/src/mame/capcom/gng.cpp#L211-L220) · [MAME のバンク配置](https://github.com/mamedev/mame/blob/mame0289/src/mame/capcom/gng.cpp#L496-L500)

## 起動と割り込み

| 起点 | 種別 | 確認した内容 |
| --- | --- | --- |
| fixed:`$FFF2–$FFFF` | ベクタ表 | IRQ は `$FFF8 → $65C4`、リセットは `$FFFE → $6000`。残りの5語は `$FFFF`。 |
| fixed:`$6000`、`$6073` | コード | リセットから初期化に入り、`$6073` で bank3 を選び、`$607A` から `bank3:$553A` の自己診断に移る。 |
| bank3:`$553A`、`$55F9–$560B`、`$560C–$5659` | コード | 自己診断が `$560C` から78バイトを RAM の `$1A00–$1A4D` にコピーして実行する。コピー先の `$1A0D` が bank4→3→2→1→0 の順で窓を切り替える。チェックサム照合のため bank3 への切り替えも間に入る。 |
| fixed:`$60DA–$60FB`、bank4:`$4000–$40FF` | コード、コピー元のデータ | bank4 を選択してから、各128バイトを RAM の `$0480` と `$16B2` へコピーする。 |
| fixed:`$65C4`、`$65E8–$65FA` | コード | IRQ の入口からスプライトバッファ書き込みや更新処理に進む。bank3 を選択して `$65F3` で間接呼び出しし、`$D9` の値でバンクを戻して `RTI`。 |
| fixed:`$65BC–$65C3` | IRQ の呼び出し先テーブル | `$02` を2倍して参照するビッグエンディアンのアドレス列。先頭3件の fixed:`$684F`、fixed:`$694D`、fixed:`$6F22` は起動から1P開始までに実行時の参照を確認した。4件目の `bank3:$5671` は静的に確認した。 |
| fixed:`$65FB–$660C`、`$660D–$6618` | コード、2つのテーブル | `$21 & 3` を使い、`$660D` からバンク選択値を読み、`$6611` からジャンプ先を得る。`$6604` の書き込みで値3と4を観測した。 |

## 次に参照する処理

| 位置 | 種別 | 用途と根拠 |
| --- | --- | --- |
| fixed:`$6309`、`$633F`、`$6353` | コード | 順に bank2、0、1 を選ぶ。bank2 の `$4200` を基点にしたポインタ参照、bank0 からのインデックス読み出し、bank1 からの8バイトの読み出しへ続く。バンク選択は1P開始時に観測した。 |
| fixed:`$6D26`、`$6CEA–$6CED` | コード、ポインタ表 | 復帰フラグ `$81` で参照する表を選び、区間番号 `$72` を使って復帰処理が読む4バイトのレコードを選ぶ。 |
| fixed:`$E3E5–$E3EC`、`$E3ED–$E3F0` | コード、ジャンプ先テーブル | オブジェクトのフィールドを2倍して参照し、`$E3CA` または `$E3F1` に分岐する。移動足場の更新の調査に使える。 |

表にない領域は未分類。全域のリストは先頭から順にデコードしているため、データを命令として表示する行や、テーブル直後で命令境界がずれる行がある。分岐先やベクタで入口が分かるときは、そのアドレスから再度逆アセンブルする。[復帰処理の実測](revision-g.md#第1面の途中復帰) · [足場の実測](revision-g.md#移動足場の接地判定と三つ目の水路)

## 再現手順

```powershell
$mameDir = "$env:USERPROFILE/tools/mame/mame0289"
python tools/mame/disassemble-revg.py --unidasm "$mameDir/unidasm.exe"
& "$mameDir/unidasm.exe" build/revg-disassembly/fixed.bin -arch m6809 -basepc 0x65c4 -skip 0x5c4 -count 0xc -lower
& "$mameDir/mame.exe" makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-rom-map.lua -cfg_directory build/revg-rom-map-cfg -nvram_directory build/revg-rom-map-nvram -seconds_to_run 22
```

生成スクリプトは3個の ROM メンバーを MAME の SHA-1 と照合し、固定領域と5バンクの `.bin` と `.asm` を Git 管理外の `build/revg-disassembly/` に出力する。上の `unidasm` の例では、ファイル内の `0x5c4` を飛ばし、出力と相対分岐の基準を CPU アドレス `0x65c4` に置く。

MAME のログでは `bank_select` が書き込み値とバンク、`vector_read` がベクタの読み出し、`irq_dispatch` がテーブルで参照された分岐先を示す。ログの PC はタップ時点の値で、例えば `$6073` の書き込みは `$6076` で報告される。電源投入からの2回の実行で、速度表示を除くログが一致した。
