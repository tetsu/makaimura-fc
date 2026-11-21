# VRC7a 診断 ROM を Mesen で試す

ZIP を展開する。Mesen 2.1.1 の NES 設定を NTSC にし、1P に NES コントローラーを割り当てる。スプライト制限を有効にし、オーバースキャンを上下左右とも0にして設定を保存する。

`build/vrc7a-test-512k.nes` を Mesen で開く。メニューで **B** を押すと元のコース、**下＋B** で追加コースを開始する。十字キーで移動、**A** でジャンプ、**B** で射撃。ひし形の標識を通過すると途中復帰地点が有効になる。被弾後は **START** で再開し、右端の旗に着地するとクリア。プレイ中やクリア後は **SELECT** でメニューに戻る。

## 自動試験

展開先のルートで PowerShell を開き、Mesen.exe の場所を指定して実行する。

```powershell
./tools/test-mesen.ps1 -UseBuiltRoms -MesenPath 'C:/path/to/Mesen.exe'
```

同梱の32 KiB版と512 KiB版で回帰試験を行う。結果は端末と `build/mesen-smoke/` に出力される。両 ROM の SHA-256 は `SHA256SUMS.txt` に記載している。
