# 国内版 Revision G の挙動調査

対象は `makaimurg`、MAME 0.289、既定 DIP。MAME の ROM 照合結果は `romset makaimurg [gng] is good`。REA の ZIP 調査で得た SHA-256 は `d2a4f9e0f7139307be51371268d7ce367bf8fd9179ca188991c35ed9dee596bc`（Evidence `ev_b09fd6582bf439d008a8f42a3b19ba6d90c7226001ced8786e10db67e1eda0e8`）。最初にコールドブート後の無入力デモを測り、その後に1Pプレイの入力応答を測った。

## 再現方法

リポジトリのルートから次を実行する。番号は Lua のフレーム通知を 1 から数えた値で、描画済み画像にはバッファリングによる時間差があり得る。MAME の画面更新は 59.637405 Hz。同じ条件で2回起動し、4種類の観測ログが一致した。

```powershell
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg.lua
```

[計測スクリプト](../tools/mame/measure-revg.lua)は `motion` にフレーム、横スクロール値、表示部品の左端 X と上端 Y、部品数、作業 RAM の縦座標候補を出力する。`write_y` は縦座標への書き込み、`background` は背景 RAM の列と更新行のビットマスクを示す。書き込みの番号は直前のフレーム通知を指す。部品を選ぶ条件はこの短い区間に合わせたもので、別の姿勢・敵・面には再検証が必要。

## 動きと地形接触

| 観測フレーム | スクロール X | 表示左端 X | 表示上端 Y | RAM $050A |
| --- | ---: | ---: | ---: | ---: |
| 1261 | 82 | 112 | 178 | 62 |
| 1278 | 98 | 112 | 138 | 102 |
| 1291 | 114 | 112 | 159 | 81 |
| 1318 | 141 | 112 | 178 | 62 |

- **横移動とカメラ。** フレーム1165から1185までに左端 X が89から110へ移り、横スクロール値は0のままだった。1190から1260までは左端 X が112付近に留まり、横スクロール値が2から81へ進んだ。この区間の地形に対する平均移動量は約1.13 px/フレーム。歩行とカメラ追従は別々に比較する。
- **上昇と着地。** この動作の表示上端は178から138まで40 px 上昇し、1291〜1314の24サンプルで159を保ち、1318で178に戻った。平地より19 px 高い位置にいったん乗った動きと整合する。この1回から平地間の滞空時間や全ジャンプ共通の高さは決められない。
- **縦座標補正。** 通知1291の後、作業 RAM `$050A` に78を書いた後で81を再度書く動作を観測した。書き込み時に報告された 6809 の PC は順に `$94C3` と `$9E84`。後続の計測で、墓石に接地したときの補正と確認した。[墓石の接地境界](#墓石の接地境界)
- **接触時の比較。** 日本版 G の固定 ROM を MAME 同梱の `unidasm` で調べると、`$9E52–$9E66` では2つの差にオフセットを加え、どちらかが符号なし上限を超えると次の候補へ進む。縦座標の書き戻し命令は `$9E82` で、タップが報告する `$9E84` はその直後。ログの `contact` はフレーム、X・U レジスタ、比較に使う4バイトの順。1291と1292では X=`$0510`、U=`$1390`、値は順に12・24・19・38だった。

## ステージと背景更新

観測区間1190〜1329では、背景 RAM の列17から25までを計9回、毎回32行ずつ更新した。更新間隔は12〜18フレーム。この結果は、FC側で横スクロール時に列転送が集中する試験を組む根拠になる。ここでの列番号は循環する表示用 RAM の位置で、ステージ全体の長さや地形判定の配列位置は別途調べる。[MAME の背景タイルと描画の定義](https://github.com/mamedev/mame/blob/master/src/mame/capcom/gng.cpp)

## 1P プレイ開始直後の入力応答

MAME 0.289 の既定 DIP で、コールドブートから通知400〜403でコイン、500〜503で1Pスタートを入力した。冒頭の演出後、最初の入力を通知1140から与えた。無入力を含む8条件をそれぞれ2回起動し、入力・RAM・音コマンドの全ログが条件ごとに一致した。分岐直前の座標は全条件で X=96、縦座標=62、スクロール=0だった。

```powershell
$env:REVG_TRIAL = 'forward'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-input.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 22
Remove-Item Env:REVG_TRIAL
```

[入力計測スクリプト](../tools/mame/measure-revg-input.lua)の条件は `idle`、`walk`、`reverse`、`crouch`、`shot`、`jump`、`forward`、`simultaneous`。比較する際は `trial,`、`type,`、`input,`、`sample,`、`sound,` から始まる行を抽出する。入力と音コマンドの番号は直前に処理されたフレーム通知を示し、`sample` の番号は RAM を読んだ通知を示す。応答時間は CPU が新しい入力を最初に読んだ番号と観測値が変わった番号の差で、1フレームは約16.8 ms。描画済み画像の応答時間はこの測定に含めていない。[MAME 0.289 の入出力マップ](https://github.com/mamedev/mame/blob/mame0289/src/mame/capcom/gng.cpp#L233-L250) · [MAME の Lua 入力 API](https://docs.mamedev.org/luascript/ref-input.html#methods-3)

| 操作 | 初めて読んだ番号 | 変化を観測した番号 | 差 |
| --- | ---: | --- | ---: |
| 右に歩き始める | 1141 | 1143で X が増加 | 2フレーム |
| 右を放す | 1156 | 1157で X の増加が止まる | 1フレーム |
| 右から左へ切り替える | 1156 | 1157で X が据え置き、1158で減少 | 左移動まで2フレーム |
| 下を6フレーム押す | 1141 | 1143でしゃがみ状態に入り、1162で解除 | 開始まで2フレーム |
| 射撃ボタンを1フレーム押す | 1141 | 1142で弾がスプライト RAM に現れ、射撃の音コマンドも出る | 1フレーム |
| ジャンプボタンを1フレーム押す | 1141 | 1143でジャンプ状態、1144で縦座標が上昇 | 状態変化まで2フレーム、座標変化まで3フレーム |

歩行の X は `$0507:$0508` で、右移動中は1フレームあたり1.125 px増えた。しゃがみフラグ `$051F` は、下の解放を読んだ1147から15フレーム後の1162で戻った。

平地でジャンプ状態 `$0504` が立ってから接地に戻るまでは、垂直・前進とも35フレーム（約587 ms）だった。縦座標 `$050A` は62から最大101へ変化し、整数座標で39 px上昇した。右を先に押し、4フレーム後にジャンプした条件では、踏み切りから着地までに X が100から138へ進み、カメラも9 px追従した。立ち姿のスプライト枠の幅32 pxに対して約1.19倍。右とジャンプを同時に押した条件では、着地時の X は開始時と同じだった。この比較は静止状態から右へ進む最初の1回に限る。

## 1P プレイの先頭のゾンビ

MAME 0.289 の既定 DIP、1周目の開始地点で、右から接近する最初のゾンビを追跡した。9条件をコールドブートから2回ずつ実行し、同じ条件の計測ログが一致した。

```powershell
$env:REVG_ENEMY = 'intro_left'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-enemy.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 25
Remove-Item Env:REVG_ENEMY
```

[敵の計測スクリプト](../tools/mame/measure-revg-enemy.lua)は、プレイヤーと敵のワールド X を256分の1 px単位で出力する。敵のレコード `$0DF0` は、左へ進むワールド座標とカメラ位置からスプライト RAM 上の歩行パターンの位置を照合して選んだ。`sprite_in_view` は該当するスプライト枠が表示域と交差したことを示す。描画のバッファリングやパターン内の透明部分による画素の遅れは含まない。被弾時刻にはプレイヤーの鎧状態 `$0500` が1から2に変わる通知を使い、翌フレームには音コマンド `$01` も観測した。[MAME 0.289 のスプライト描画](https://github.com/mamedev/mame/blob/mame0289/src/mame/capcom/gng.cpp#L176-L208)

- **出現と歩き始め。** 無入力では通知1063で X=272にレコードが出現し、1069で左へ移動し始め、1071で歩行スプライト枠が表示域に入った。左を早くから押した `intro_left` では出現時のプレイヤー X が96.1から93.9に変わったが、出現・歩き始め・表示域に入る時刻は一致した。1Pスタートだけ60フレーム遅らせた `late_start` では、出現から無入力時の初回被弾までの経過が60フレームずれた。発生条件に位置の範囲が含まれるかは未確認。
- **接近と被弾。** この個体は歩き始めから最初の被弾まで毎フレーム208/256 px（0.8125 px）ずつ左へ進んだ。被弾の前後でゾンビの移動速度と記録した状態は変わらず、独立した攻撃準備への遷移は観測されなかった。静止3条件の被弾フレームでは、縦座標は双方62、敵の X はプレイヤーの約8.2〜9.2 px右だった。この間隔は整数フレームで観測した値で、当たり判定の境界は別に測る。

| 条件 | 初回被弾の通知 | 観察された差 |
| --- | ---: | --- |
| `idle` 無入力 | 1274 | X=96.1で待機 |
| `wait_ahead` 前に進んで待機 | 1248 | X=117.5で待機し、26フレーム早い |
| `wait_behind` 後ろに下がって待機 | 1316 | X=61.3で待機し、42フレーム遅い |
| `early` 通知1140から右 | 1198 | こちらから接近すると76フレーム早い |
| `delayed` 通知1180から右 | 1222 | 右入力を40フレーム遅らせると、`early` より24フレーム遅い |

通知1260で垂直ジャンプした条件では、1274の X は無入力時と同じで、プレイヤーの縦座標は95、敵は62だった。この時点では被弾せず、降下中の1294で被弾した。通知1220で撃つと、ゾンビは1228で歩行から別の状態へ移って停止し、1250でスロットが空いた。無入力時の1274では被弾せず、スロットの再使用後に1318で被弾した。

## ゾンビ本体と初期武器の判定境界

先頭のゾンビについて、プレイヤーが立ち・しゃがみ・空中にいるときの被弾判定と、立ち状態から撃った初期武器の命中判定を測った。MAME のメモリ読み出しフックで、通知1159の後に判定処理が読み取る敵の整数座標だけを差し替えた。座標は RAM に書き戻さず、地形補正や歩行処理は通常どおり進む。差は敵座標からプレイヤーまたは弾の座標を引いた値で、X の正は右、Y の正は上。もう一方の軸の差を0に固定し、各境界の内側と1 px外側で判定結果を照合した。[MAME の読み出しフック](https://docs.mamedev.org/luascript/ref-mem.html)

| 観測条件 | 判定内の X 差 | 判定内の Y 差 |
| --- | ---: | ---: |
| 立ちでゾンビに接触 | −9〜+9 px | −19〜+19 px |
| しゃがみでゾンビに接触 | −9〜+9 px | −19〜+13 px |
| 垂直ジャンプ降下中にゾンビに接触 | −9〜+9 px | −19〜+19 px |
| 初期武器がゾンビに命中 | −12〜+12 px | −18〜+10 px |

範囲は両端を含む。しゃがみ時は、敵がプレイヤーより上にいる側だけ6 px縮んだ。X 差0、Y 差+14 pxはしゃがみで判定範囲の外になる。被弾は `$0500` の変化、命中は対象の `$0DF3` の変化で確認した。

固定 ROM を MAME 同梱の `unidasm` で調べ、ゾンビ本体の `$F087–$F09B` と初期武器の `$EEF8–$EF0C` で、各軸の差にオフセットを加えて符号なし上限と比較することを確認した。比較表は立ち・空中が `$F122`、しゃがみが `$F1BA`、この武器とゾンビの組み合わせが `$EFF4` だった。表は両軸を比較する処理と実測した境界をまとめたもので、スプライトの外形や地形の接触範囲は含まない。[MAME 0.289 の固定 ROM 定義](https://github.com/mamedev/mame/blob/mame0289/src/mame/capcom/gng.cpp#L980-L984)

無入力で被弾からの復帰も観測した。本体判定が通知1273の後に被弾を書き込み、1307の後に `$0500` が1に戻った。次に本体判定ルーチンが鎧状態を読んだのは1393の後で、最初の被弾から120フレーム（約2.01秒）、鎧状態が戻ってから86フレーム後だった。この時間は開始地点での本体判定の再開までを指す。

```powershell
$env:REVG_CONTACT = 'crouch'
$env:REVG_DX = '0'
$env:REVG_DY = '14'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-contact.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 25
Remove-Item Env:REVG_CONTACT, Env:REVG_DX, Env:REVG_DY
```

[境界計測スクリプト](../tools/mame/measure-revg-contact.lua)で `stand`、`crouch`、`air`、`shot` を指定し、表の各端とその1 px外側を検証できる。体の接触では `hit` が被弾、弾では命中を示す。`probe` は観測した軸・差・基準座標・表のアドレス・オフセットと上限を出力する。`recovery` は入力と座標を変えず、被弾から本体判定の再開までの `armor` と `gate` を出力する。計32境界条件と復帰を2回ずつ実行し、条件ごとに計測ログが一致した。

## 墓石の接地境界

MAME 0.289 の既定 DIP で、最初の無入力デモが越える墓石を調べた。墓石のレコードは `$1380`、判定時の U レジスタは `$1390`。足場の基準はワールド X=256、Y=62で、接地時はプレイヤーの Y が81に揃う。Y の正は上。

| 通知直後の判定 | プレイヤー X | 墓石との X 差 | 観測結果 |
| ---: | ---: | ---: | --- |
| 1290 | 243 | −13 px | 横の比較で判定範囲から外れた |
| 1291 | 244 | −12 px | 下降中の Y が78から81に補正されて接地 |
| 1312 | 268 | +12 px | Y=81で接地を維持 |
| 1313 | 269 | +13 px | 接地を失い、次の更新で下降 |

墓石を離れて4フレーム後の通知1317の後に、通常地面への接地が成立した。翌サンプルで空中状態が解除された。墓石から地面への高さの差は19 pxだった。[通常地面の着地境界](#通常地面の着地境界)

通知1291の判定だけを使う境界試験では、MAME の読み出しフックで `$052B` または `$052D` の値を1回の座標比較に渡すときに差し替えた。横は墓石から±12 pxで接地補正があり、±13 pxで補正がなかった。縦比較に使う補助座標では墓石から±19 pxで補正があり、±20 pxで補正がなかった。通常の RAM とプレイヤーの現在座標を維持しているため、縦の値はこの比較が受け付ける範囲を表す。下から進入した場合や、通常地面・壁・穴の境界には別の試験が必要。[MAME の読み出しフック](https://docs.mamedev.org/luascript/ref-mem.html)

```powershell
$env:REVG_TERRAIN = 'x'
$env:REVG_OFFSET = '-13'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-terrain.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 25
Remove-Item Env:REVG_TERRAIN, Env:REVG_OFFSET
```

[地形計測スクリプト](../tools/mame/measure-revg-terrain.lua)は `natural` で無介入のデモを追い、`x` または `y` で片軸だけ差し替える。ログの `compare` は軸、元の座標の下位バイト、墓石の基準座標、有効な下位バイト、符号付きの座標差の順。比較と書き込みの番号は直前のフレーム通知を指す。無介入と境界8条件の各2回でログが一致した。

## 通常地面の着地境界

同じ無入力デモで、墓石を降りた先の平地を調べた。空中状態のプレイヤーは通知1317の時点で X=269、Y=67にいた。続く更新で落下処理が Y=62に進め、地面の判定が成立すると次の通知1318で空中状態が解除された。

固定 ROM の `$8B0A` では、この姿勢の足元をプレイヤーの X−8 px、Y−16 pxで調べ、接地できなければ16 px右の候補を調べる。この場面の候補は X=261と277。2つの背景タイルはいずれも番号 `$06`、属性 `$09` で、タイル内の高さの上限は14だった。Y の正は上で、判定が読む Y=62はタイル内の高さ14、Y=63は15に相当する。

| 条件 | 左側の判定 | 右側の判定 | 次の通知 |
| --- | --- | --- | --- |
| `natural` 無介入 | 高さ14で接地 | 比較なし | 接地 |
| `above` 読み取る Y を1 px上げる | 高さ15で範囲外 | 高さ15で範囲外 | 空中 |
| `right` 左側だけ属性で除外 | 属性で除外 | 高さ14で接地 | 接地 |
| `neither` 両側を属性で除外 | 属性で除外 | 属性で除外 | 空中 |

MAME の読み出しフックを通知1317の判定だけに適用した。`above` は地面判定が読む整数 Y だけを差し替え、`right` と `neither` は該当タイルの属性の上位2ビットを立てた。背景 RAM やプレイヤーの実座標への書き込みは行っていない。接地は書き込みフックが報告する PC `$9054` と、次の通知での空中状態の解除を照合した。4条件をコールドブートから2回ずつ実行し、条件ごとにログが一致した。[MAME の読み出しフック](https://docs.mamedev.org/luascript/ref-mem.html)

```powershell
$env:REVG_GROUND = 'right'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-ground.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 25
Remove-Item Env:REVG_GROUND
```

[地面計測スクリプト](../tools/mame/measure-revg-ground.lua)の `tile` は足元の左右、背景タイルのアドレス、タイル番号、元の属性、判定に渡した属性を示す。`depth` はタイル内の高さとその上限。

### 片側が途切れる足場

同じ平地の判定で、ワールド X=272 を境に片側のタイルだけを有効にした。境界の近くで整数 X を判定に渡し、両側で1 px違う条件を比べた。Y の比較には無介入と同じ62を使った。

| 有効な側 | 接地した中心 X | 空中のままの中心 X |
| --- | ---: | ---: |
| 左側 `edge_left` | 279 | 280 |
| 右側 `edge_right` | 264 | 263 |

左足は X=271から272に移った時点で左側のタイルを外れ、右足は X=271から272に移った時点で右側のタイルに入った。再現するには上のコマンドで `REVG_GROUND` を表の条件に変え、`REVG_GROUND_X` に比較する中心 X を指定する。各条件を2回実行し、ログが一致した。実際の穴での落下と復帰も後続の試験で測った。[水面の穴と復帰](#水面の穴と復帰)

## 壁の左右判定

右移動は墓石を降りた後の無入力デモ（通知1320）、左移動は1P開始後の左歩行（通知1160）で測った。固定 ROM の `$8B7D` と `$8BAF` は、それぞれプレイヤーの右・左16 px、下8 pxの背景を調べる。通行を妨げない元の地点で、判定が読むタイル番号だけを左右の境界値が8の `$2E` に差し替え、属性の上位2ビットをクリアした。タイル内 X の比較値を7、8、9と変え、実際の X の書き込みで接触を判定した。

| 移動方向 | タイル内 X=7 | X=8 | X=9 |
| --- | --- | --- | --- |
| 右 | 通過 | 接触、補正0 px | 接触、左へ1 px補正 |
| 左 | 接触、右へ1 px補正 | 接触、補正0 px | 通過 |

```powershell
$env:REVG_WALL = 'right'
$env:REVG_WALL_X = '9'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-wall.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 25
Remove-Item Env:REVG_WALL, Env:REVG_WALL_X
```

[壁計測スクリプト](../tools/mame/measure-revg-wall.lua)で方向を `left` または `right` に変更できる。`REVG_WALL_X` を省略すると無介入に戻る。両方向の無介入と6つの境界条件を各2回実行し、条件ごとにログが一致した。[MAME の読み出しフック](https://docs.mamedev.org/luascript/ref-mem.html)

## 敵弾の発射と被弾境界

1P開始後、右を保持し、通知番号が49の倍数のときにジャンプ、22の倍数のときに射撃して進んだ。通知1845の後に段上の植物型の敵が弾を生成した。発射元は (816, 138)、次の通知での弾の座標は (815, 143)。最初の弾は左下へ進み、両軸とも毎フレーム408/256 pxずつ座標が減った。通知1874の後、空中のプレイヤーに敵弾が当たり、鎧状態が変わった。この判定に渡された座標はプレイヤー (774, 94)、弾 (768, 96) だった。ここまでの再現はコントローラー入力だけで行った。

同じ弾について、立ち・しゃがみは通知1854から右入力を止め、しゃがみでは下を押した。通知1874の被弾判定が読む弾の座標だけを差し替え、境界の両側を24条件で測った。差は弾からプレイヤーを引いた値で、Y の正は上。

| 姿勢 | 被弾する X 差 | 被弾する Y 差 |
| --- | ---: | ---: |
| 立ち・空中 | −6〜+6 px | −8〜+8 px |
| しゃがみ | −6〜+6 px | −8〜+2 px |

両端を含む。無介入の被弾後、鎧状態は通知1893の後に1へ戻り、敵弾の判定は通知1994の後に再開した。最初の被弾から120フレーム（約2.01秒）後だった。

同じ入力を続けると、通知2635の後にレッドアリーマーが種別7の弾を生成した。発射時の敵は (1525, 62) にいて、弾は毎フレーム578/256 px左へ進み、縦位置は維持された。通知2640では弾が右に34 px離れており、被弾しなかった。同じ判定に座標差 (0, 0) を渡すと被弾した。植物型の敵の発射条件は後続の試験で確認した。[植物型の敵の初弾](#植物型の敵の初弾)

```powershell
$env:REVG_PROJECTILE = 'crouch'
$env:REVG_DX = '0'
$env:REVG_DY = '3'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-projectile.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 46
Remove-Item Env:REVG_PROJECTILE, Env:REVG_DX, Env:REVG_DY
```

[敵弾計測スクリプト](../tools/mame/measure-revg-projectile.lua)では、両オフセットを省くと入力だけで再現する。`air` は植物型の敵の弾による被弾、`recovery` はその後の判定再開、`followup` は種別7の弾を追う。`sample` の弾の座標は1/256 px単位、`compare` は整数座標と判定に渡した差を示す。[MAME の読み出しフック](https://docs.mamedev.org/luascript/ref-mem.html)

### 植物型の敵の初弾

1P開始後、前節と同じ経路を使い、ワールド X=816にいる植物型の敵の攻撃開始時刻を測った。通知番号は読み出し・発射の直前のもの。攻撃開始は、カメラに対する範囲判定を通って敵の攻撃段階が変わった更新を指す。

| 入力 | 攻撃開始 | 初弾 | 初弾時のプレイヤー X |
| --- | ---: | ---: | ---: |
| `baseline` そのまま接近 | 1755 | 1845 | 762 |
| `wait_inside` 範囲内で待機 | 1755 | 1845 | 688 |
| `delay` 範囲の手前で64フレーム待って接近 | 1799 | 1889 | 745 |
| `wait_outside` 範囲の手前で待機 | 観測なし | 通知2018まで発射なし | — |

発射した3条件は攻撃開始から90フレーム（約1.51秒）で初弾を生成した。接近の途中に64フレームの待機を入れた条件では、カメラが範囲に入った更新が44フレーム遅れ、発射も44フレーム遅れた。

固定 ROM の `$8CA4` は敵とカメラの位置をX・Yの両方で比較する。通知1755の横比較でカメラ X の読み出しだけを差し替えると、**552と824で範囲内、551と825で範囲外**になった。このときの横の範囲は敵の X−264からX+8までで、両端を含む。元のカメラと敵の座標は保持した。初回の比較を範囲外にした2条件では、次の範囲内の比較が通知1759の後に起き、発射も1849にずれた。

発射処理 `$DEBF` は弾の空き枠数 `$00A7` を読む。通常の初弾では6枠空いていた。この読み出しだけに0を渡す `no_slot` では通知1845の発射が起きず、枠がある状態に戻っても通知2018まで発射はなかった。初弾の再現はこの敵と1周目の当該区間に限った。画面から離れた後の再開は未測定。

```powershell
$env:REVG_PLANT = 'delay'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-plant.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 36
Remove-Item Env:REVG_PLANT
```

[発射条件の計測スクリプト](../tools/mame/measure-revg-plant.lua)で表の入力条件と `no_slot` を選べる。横境界を測る場合は `REVG_PLANT=baseline` とし、`REVG_PLANT_CAMERA_X` に551、552、824、825のいずれかを指定する。入力4条件・枠不足・横境界4条件を2回ずつ実行し、条件ごとにログが一致した。REA の接続先では 6809 の生 ROM を直接開けなかったため、命令の確認には MAME 同梱の `unidasm` を使った。[MAME の読み出しフック](https://docs.mamedev.org/luascript/ref-mem.html)

### レッドアリーマーの初弾

1周目で先ほどと同じ入力経路を先へ進み、種別7の弾を撃つレッドアリーマーを追った。`wait` と `pause` は通知2520から方向・ジャンプ・射撃の入力を止める。`pause` は64フレーム後に入力列を再開し、`wait` は計測終了の2750まで入力を止めたままにした。

| 入力 | 発射準備 | 初弾 | 初弾時の敵座標 | 初弾時のプレイヤー座標 |
| --- | ---: | ---: | --- | --- |
| `baseline` 接近を継続 | 2628 | 2635 | (1525, 62) | (1474, 62) |
| `wait` その場で待機 | 2648 | 2655 | (1412, 127) | (1371, 62) |
| `pause` 64フレーム待って再接近 | 2736 | 2743 | (1353, 134) | (1515, 81) |

この3条件では地上・空中で発射し、プレイヤーが左右どちらにいても発射準備から初弾までは7フレーム（約117 ms）だった。発射準備に入る前の移動段階は、順に0・1・5だった。

MAME 同梱の `unidasm` と実行ログで、待ち時間のカウンタ `$0EC6` が0になると `$B7B0` から発射準備に入り、`$B9B4` で準備用のカウンタを6に設定することを確認した。`early_timer` では通知2624に待ち時間の読み出しだけを4から0に変えた。発射準備が2628から2624へ、初弾が2635から2631へ、それぞれ4フレーム早まった。

`no_slot` では発射処理の空き枠の読み出しだけを通知2635〜2637で0にした。3フレーム連続で空きを調べ、2638に元の値6を読んだ更新で発射した。待ち時間の選択や回避動作による発射準備の中断を含め、別の周回・個体での条件は未測定。

```powershell
$env:REVG_ARREMER = 'no_slot'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-arremer.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 48
Remove-Item Env:REVG_ARREMER
```

[発射計測スクリプト](../tools/mame/measure-revg-arremer.lua)では `ready` が準備に入った通知、`windup` が準備用カウンタの設定、`capacity` が空き枠の元の値と有効値、`launch` が弾生成を示す。入力3条件と読み出しを変えた2条件を各2回起動し、条件ごとにログが一致した。

## FC 試験コースとの比較

自作の VRC7a 試作を Mesen 2.1.1 の NTSC 設定で起動し、STARTで入る横スクロールの操作試作を測った。32 KiB版と512 KiB版でログは一致した。ジャンプの距離は、ジャンプ状態に入った更新から接地した更新までのワールド X の差。実時間は MAME の59.637405 Hz、Mesen の約60.1 Hzで換算した。

| 観測項目 | Revision G | FC 試作 |
| --- | --- | --- |
| 平地の移動速度 | 1.125 px/フレーム、約67.1 px/秒 | 1 px/フレーム、約60.1 px/秒 |
| 歩き始め・反転 | 最初の入力読み出しから2フレーム | 読んだ入力と同じ更新で移動 |
| 停止・射撃 | 最初の入力読み出しから1フレーム | 読んだ入力と同じ更新で停止・発射 |
| 垂直・前進ジャンプの状態継続 | 35フレーム、約587 ms | 33フレーム、約549 ms |
| 最高到達点 | 地面から39 px | 地面から40 px |
| 前進ジャンプでの水平距離 | 右を先に押すと約38 px | 右とジャンプの同時入力で33 px |

静止状態から右とジャンプを同時に押すと、Revision G の測定条件では X が変わらず、FC では前進した。応答の記録には表示や入力機器の遅延を含めていない。FC の試作にはしゃがみと敵弾がなく、地形判定は中心 X の列を使うため、これらの条件比較は未実施。

[FC 操作計測スクリプト](../tools/mesen/measure-player.lua)は同名の ca65 デバッグシンボルを読み、Lua コンソールに入力と座標、離陸・着地時刻、最高到達点、弾の生成時刻を出力する。1P に NES コントローラーを割り当てる。Windows でツールが PATH にある場合は次の手順で32 KiB版を測れる。512 KiB版では ca65 に `-D 'PRG_ROM_UNITS=32'`、ld65 に `-D 'PRG_PADDING=$78000'` を加える。[Mesen の Lua API](https://github.com/SourMesen/Mesen2/blob/master/UI/Debugger/Documentation/LuaDocumentation.json)

```powershell
New-Item -ItemType Directory -Force build | Out-Null
ca65 -g src/vrc7a-test.s -o build/player-measure.o
ld65 -C src/vrc7a-test.cfg -o build/player-measure.nes --dbgfile build/player-measure.dbg build/player-measure.o
Mesen.exe --testRunner --timeout=20 tools/mesen/measure-player.lua build/player-measure.nes
```

## 水面の穴と復帰

MAME 0.289、既定 DIP の1周目で、墓地の先にある水面までコントローラー入力で進んだ。右と一定間隔での射撃を続ける2経路を比較した。`jump` は穴までジャンプを繰り返し、`walk` は通知2650からジャンプを止めた。岸の手前で右入力を1フレーム早く放した `shore` では、中心 X=1655、Y=61で通知3052まで接地を保った。1フレーム遅く放した `walk` は中心 X=1657に進んで接地を失い、続く更新で Y が下がった。

同じ更新の地面判定に渡す整数 X だけを差し替えると、**X=1655では接地、X=1656では空中**になった。X=1655では左側の候補が岸のタイル `$08` を参照した。X=1656では左右の候補が `$B3` と `$00` に移った。差し替えを解除した次の更新では、実座標の X=1655で再び接地した。

| 測定点 | `jump` | `walk` |
| --- | ---: | ---: |
| 落下が止まり死亡演出が始まる通知 | 2837 | 3043 |
| その時点のプレイヤー座標 | (1668, 32) | (1657, 27) |
| その時点のプレイ用カメラ X | 1540 | 1529 |
| 復帰座標を設定した通知 | 3260 | 3466 |
| プレイ画面に戻った最初のサンプル | 3300 | 3506 |

両経路とも死亡演出の開始から423フレーム後に復帰座標が設定され、463フレーム後（約7.76秒）にプレイ画面でプレイヤー (96, 64)、カメラ X=0を観測した。区間番号 `$0072` は0で、鎧付きの状態に戻った。追跡中のレッドアリーマーのレコードは、復帰処理で (1472, 64) の未活動状態へ戻った。プレイ画面に戻ってから50フレーム後には、開始側のゾンビのスロットが両経路で再び有効になっていた。この2経路は面の開始地点に戻った。残機の処理と水面で死亡を決める厳密な条件は未測定。[第1面の途中復帰](#第1面の途中復帰)

```powershell
$env:REVG_PIT = 'walk'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-pit.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 62
Remove-Item Env:REVG_PIT
```

[落下・復帰の計測スクリプト](../tools/mame/measure-revg-pit.lua)は `jump`、`walk`、`shore` を入力だけで再現する。`edge` では `REVG_PIT_X` に1655または1656を指定する。死亡演出の開始は落下が止まった時点と音コマンド `$3F` で照合し、2フレーム後の `$31` も記録した。記録した復帰時の PC は `$6D50` と `$8F3A`。5条件を各2回実行し、ログが一致した。

## 第1面の途中復帰

MAME 0.289、既定 DIP の1周目で、前節の歩行経路から右入力を通知3033で放して岸にとどまった。3389に右を押し、3390にジャンプすると水上の移動足場に乗れた。3420から右を放し、足場に乗ったまま待った。通知3430でプレイヤーの Y=60、足場の Y=36を観測し、3945までに対岸でプレイヤーの Y が62に戻った。3950から右に進んだ。

| サンプル | プレイヤー座標 | プレイ用カメラ X | 区間 `$0072` | 途中復帰 `$0081` |
| ---: | --- | ---: | ---: | ---: |
| 4283 | (1920, 62) | 1792 | 0 | 0 |
| 4284 | (1921, 62) | 1793 | 0 | 1 |
| 4310 | (1920, 62) | 1792 | 0 | 1 |
| 4439 | (1783, 28) | 1655 | 0 | 1 |
| 4863 | (1920, 64) | 1655 | 0 | 1 |
| 4902 | (1920, 64) | 1792 | 0 | 1 |

通知4300から左に引き返すと水面に落ちた。カメラが境界より左に戻ってもフラグは1を保ち、復帰座標は (1920, 64)、復帰したプレイ画面のカメラ X は1792だった。落下の直後、通知4441に死亡演出の音コマンド `$3F` を記録した。

固定 ROM の `$ECB8–$ECDB` は第1面のこの区間でカメラ X を `$0700`（1792）と比較し、超えた更新で `$0081` に1を書く。区間番号 `$0072` は0のまま進む。復帰側の `$6D26` では区間とフラグを使って復帰データを選ぶ。前節の落下経路の通知2800で、比較が読むカメラ X だけを1792に変えた場合は開始地点 (96, 64) に戻り、1793に変えた場合は途中地点 (1920, 64) に戻った。入力だけの横断・落下、無介入の落下、境界2条件を各2回実行し、条件ごとにログが一致した。

```powershell
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-checkpoint.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 86
```

[途中復帰の計測スクリプト](../tools/mame/measure-revg-checkpoint.lua)は既定で水面を渡り、引き返して落下する。比較だけを試すには `REVG_CHECKPOINT=start` とし、`REVG_CHECKPOINT_CAMERA_X` に1792または1793を指定する。カメラ X を省略すると開始地点へ戻る通常の落下を再現する。結果は第1面のこの区間で確認した。

## 第1面のボスと区間移行

MAME 0.289、既定 DIP の1周目で、途中復帰時に2バイトの読み出しだけを差し替え、プレイヤーを (3232, 64)、カメラ X を3104に移して試行した。差し替えは通知4862の復帰処理に限り、以降はコントローラー入力で進めた。ボス撃破までは両経路で同じ入力を使った。

| 観測点 | 通知 | 状態 |
| --- | ---: | --- |
| ボスの配置 | 4904 | 座標 (3488, 64)、カメラ X=3104 |
| ボスの攻撃動作への切り替え | 5332 | プレイヤー X=3346、カメラ X=3218 |
| 10回目の有効打 | 5679 | ボスの耐久値 `$0E16` が1から0へ |
| 撃破処理 | 5766 | ボスが消え、`$0008` が2へ。カメラ X=3299 |
| 鍵の出現 | 5769 | 座標 (3365, 176) |
| 鍵の着地・取得 | 5894 | 鍵 (3365, 59)、プレイヤー (3364, 67) |
| 区間番号の更新 | 6075 | `$0072` が0から1へ |
| 次の区間のプレイ開始処理 | 6416 | 開始座標 (3664, 112)、カメラ右端の設定値6912 |

ボスの耐久値は10で、試行中の有効打ごとに1減った。撃破後に右へ歩き続けた経路では、カメラ X が3329で止まり、プレイヤー X=3576で通知6500まで区間0のままだった。右方向の更新処理 `$8015–$801A` は移動前の整数 X と上限 `$005A=3328` を比較する。この試行では3327から3329に進んだ更新が最後だった。

鍵の出現処理 `$72D8–$72EF` は撃破後にプレイヤー X が `$005A - 48`（3280）以上になるのを待ち、目標 X を `$005A + 37`（3365）に設定する。引き返した経路では、`$7315–$733E` が目標 X と着地後の鍵の当たり判定用 Y からそれぞれ ±8 px 以内で取得を記録した。181フレーム後に `$73A1` が区間番号を更新した。通知6500には次の区間のカメラ X が3584に切り替わっていた。

```powershell
$env:REVG_STAGE1_END = 'key'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-stage1-end.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 111
Remove-Item Env:REVG_STAGE1_END
```

[第1面終盤の計測スクリプト](../tools/mame/measure-revg-stage1-end.lua)は既定の `key` で撃破後に引き返して鍵を取る。`straight` では鍵を通り過ぎ、区間0の右端で待つ。2条件を各2回実行し、条件ごとにログが一致した。

### コントローラー入力だけの再現

電源投入から通常の途中復帰を2回経て林を抜け、3機目でボスを倒して鍵を取った。`controller` は MAME の入力ポートだけを操作し、観測用フックでゲームの値を変更しない。

| 観測点 | 通知 | 状態 |
| --- | ---: | --- |
| スクロールでページ12を読み込む | 7416 | プレイヤー X=3184、更新後のカメラ X=3056 |
| ボスを配置 | 7419 | ボス (3488, 64)、プレイヤー X=3188、カメラ X=3060 |
| 攻撃動作へ切り替え | 7604 | プレイヤー X=3375、カメラ X=3247 |
| 10回目の有効打 | 7775 | ボスの耐久値が0に |
| 撃破処理 | 7862 | ボスが消え、鍵の出現待ちに入る |
| 鍵を取得 | 7989 | プレイヤー (3364, 67) |
| 区間番号の更新 | 8170 | `$0072` が0から1へ |
| 次の区間のプレイ開始処理 | 8511 | 開始座標 (3664, 112)、カメラ右端の設定値6912 |

配置処理 `$EAF7–$EAFB` で敵種別 `$08` がスロット `$0D90` に入った。復帰座標を変えた試行ではスロットが `$0DF0` だったため、スクリプトは種別と割り当て先を照合して追跡する。この再生では攻撃待ちカウンタは初期値2で、通知7588と7604で減り、0になった更新で攻撃動作へ移った。鍵の取得から区間番号の更新までは、復帰座標を変えた試行と同じ181フレームだった。移行演出中のカメラは X=3328 で上限に達した。電源投入から2回再生し、ログが一致した。

```powershell
$env:REVG_STAGE1_END = 'controller'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-stage1-end.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 148
Remove-Item Env:REVG_STAGE1_END
```

## 区間1の開始と復帰

MAME 0.289、既定 DIP の1周目で、第1面のクリアから入力だけで区間1へ進んだ。区間番号が1になった通知8170に、`$6C55` の処理が途中復帰フラグ `$0081` を1から0に戻した。通知8512の開始座標は (3664, 112) で、カメラ X は通知8517までに3584へ切り替わった。同じ通知のスコア判定 `$ED46–$ED4F` で残機カウンタ `$0060` が1から2へ増えた。

| 観測点 | `section1-fall` | `section1-jump` |
| --- | --- | --- |
| 入力 | 入口から右へ歩く | 右へ進みながらジャンプと射撃を繰り返す |
| 死亡処理に入る | 通知8566、(3704, 24) | 通知9324、(4225, 230) |
| 死亡時のカメラ X | 3584 | 4097 |
| 残機カウンタが2から1へ減る | 8686 | 9444 |
| 復帰座標 (3664, 112) を観測 | 9029 | 9787 |
| 復帰後のカメラ X=3584 を観測 | 9034 | 9792 |

どちらも死亡処理に入ってから463フレーム後に、復帰座標 (3664, 112) を観測した。区間番号は1、途中復帰フラグは0で、タイマーは開始時と同じ `$005A` に戻った。区間1への初回進入と今回の2経路の復帰では、`$6D26` の処理がいずれも `$6CF2–$6CF5` の同じ4バイトを読んだ。`section1-jump` では座標が戻った最初のサンプルに死亡時のカメラ X=4097 が残り、5フレーム後のサンプルで3584を確認した。

```powershell
$env:REVG_STAGE1_END = 'section1-fall'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-stage1-end.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 171
Remove-Item Env:REVG_STAGE1_END
```

`section1-jump` に変えるとジャンプ経路を再現する。両経路を電源投入から各2回実行し、条件ごとにログが一致した。

## 区間1の途中復帰

固定 ROM の `$ECB8–$ECBD` は、区間1のカメラ X を `$1200`（4608）と比較する。`section1-fall` の通知8530で、その比較が読むカメラ X だけを4608または4609に差し替えた。この時点の実カメラ X は両方とも3584だった。

| 比較に渡した X | 死亡時の `$0081` | 復帰データの読み出し先 | 復帰座標 | 復帰後のカメラ X | 復帰時のタイマー |
| ---: | ---: | --- | --- | ---: | --- |
| 4608 | 0 | `$6CF2–$6CF5` | (3664, 112) | 3584 | `$005A` |
| 4609 | 1 | `$6D0E–$6D11` | (4736, 80) | 4608 | `$0096` |

4609を渡した更新で `$ECDB` がフラグを1に設定し、タイマー `$00AA` に `$0096` を設定した。以降の入力と実カメラは同じで、どちらも通知8566に (3704, 24) で死亡した。復帰座標は通知9029、復帰後のカメラ X は通知9034に記録した。2条件を電源投入から各2回実行し、条件ごとにログが一致した。

```powershell
$env:REVG_STAGE1_END = 'section1-fall'
$env:REVG_SECTION1_CAMERA_X = '4609'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-stage1-end.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 171
Remove-Item Env:REVG_SECTION1_CAMERA_X
Remove-Item Env:REVG_STAGE1_END
```

### コントローラー入力で境界を越える

`section1-cross` では電源投入からコントローラー入力だけで進めた。通知9721にカメラ X=4608、プレイヤー X=4736を記録し、その通知の後の更新でカメラ X=4609、プレイヤー (4737, 103) になり、途中復帰フラグが1になった。

左へ引き返して通知9862にカメラ X=4492へ戻っても、フラグは1を保った。カメラ X=4360で時間切れを待つと、通知19377に (4368, 71) で死亡処理に入った。463フレーム後に (4736, 80) へ戻り、5フレーム後にはカメラ X=4608を観測した。区間番号は1のままで、タイマーは `$0096` に戻った。電源投入から2回実行し、ログが一致した。

```powershell
$env:REVG_STAGE1_END = 'section1-cross'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-stage1-end.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 355
Remove-Item Env:REVG_STAGE1_END
```

## 区間1のボスと区間移行

MAME 0.289、既定 DIP の1周目で、`section1-cross` と同じ入力を時間切れまで再生した。途中復帰処理の通知19801で、復帰データ `$6D0F` の読み出しだけを `$12` から `$1A` に差し替えた。復帰座標は (6784, 80) で、最初のボスを配置したときのカメラ X は6658だった。

| ボス種別 | 配置の通知 | 配置座標 | 初期耐久値 |
| --- | ---: | --- | ---: |
| `$08` 先に配置される個体 | 19843 | (7040, 80) | 10 |
| `$08` 後に配置される個体 | 19845 | (7088, 80) | 10 |

対象個体にプレイヤーの弾が初めて当たった更新だけ、`$EF28` の耐久値の読み出しに1を渡した。`section1-end-one` は最初の個体を対象にし、倒した後は射撃を止めて右に進んだ。`section1-end-both` は2体それぞれに同じ差し替えを行い、撃破後に鍵へ引き返した。

| 観測点 | `section1-end-one` | `section1-end-both` |
| --- | --- | --- |
| 1体目の消滅、残数 `$00B6` が2から1へ | 通知20302 | 通知20302 |
| 2体目の消滅、残数が1から0へ | なし | 通知20486 |
| 鍵の出現 | なし | 通知20489、(6949, 176) |
| 鍵の取得 | なし | 通知20605、プレイヤー (6950, 78) |
| 区間番号の更新 | 区間1のまま | 通知20786、1から2へ |

`$E78C–$E79B` はボスの残数を減らし、0になったときに撃破後の処理へ進む。1体だけ倒した試行で、カメラは通知20421に上限設定の6912に達した。いったん左に戻ってから再び進み、通知20600にはカメラ X=6913、プレイヤー X=7159を記録した。2体を倒した試行では鍵を取ってから181フレーム後に区間番号が進み、通知21127の区間2の開始座標は (80, 576) だった。

```powershell
$env:REVG_STAGE1_END = 'section1-end-both'
mame makaimurg -rompath reference -noreadconfig -video none -sound none -nothrottle -skip_gameinfo -autoboot_delay 0 -autoboot_script tools/mame/measure-revg-stage1-end.lua -cfg_directory build/mame-cfg -nvram_directory build/mame-nvram -seconds_to_run 357
Remove-Item Env:REVG_STAGE1_END
```

`section1-end-one` に変更すると1体だけ倒す試行を再現する。2条件を各2回実行し、条件ごとにログが一致した。

## 次の比較実験

1. **区間1の通常ルート。** 途中復帰地点から入力だけで上層へ進み、ボスの配置から鍵の取得まで確認する。
2. **各面の進行。** 面と復帰地点ごとに、足場間の距離とジャンプ到達距離の比、カメラが止まる位置、敵の出現条件を記録する。
