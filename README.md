# PC-8031 Emulator 偽8031
---
# Work in progress

現在作成中のため、ドキュメントも仮です。
未実装や実装予定にない機能ついて書かれている可能性があります。

---

# これはなに

PC-8001/8801 シリーズ用の 5インチフロッピードライブ、PC-8031 シリーズのエミュレータです。
PC-8001mk2SR と PC-6001mk2SR で動作確認しています。
PC-8801 でも動くと思いますが未確認です。

---
# 配線

今回はかなり複雑なので配線などは回路図を参照ください。

![schematics 1st](/hardware/nise8031-1st.png)
![schematics 2nd](/hardware/nise8031-2nd.png)

必要なパーツは以下の通りです。

- WeAct RP2350B Core Board
- [microSD DIP 化基板](https://akizukidenshi.com/catalog/g/g105488/)
- [AQM1602Y-RN-GBW](https://akizukidenshi.com/catalog/g/g111916/)
- アクセスランプ用 LED (3mm でも 5mm でもOK)
- ロータリーエンコーダ (SW 付き)
- 抵抗 1K/10K
- アンフェノールコネクタ 34Pin
- ピンヘッダ＆ソケット(基板間接続用)

電源供給は、Pico の USB 端子を想定しています。

---
# ROM など

著作権の関係で ROM は含まれていません。

PC-80S31 (2KiB) および、PC-8801MA (8KiB) の disk.rom での動作を確認しています。

ROM ファイルを `disk.rom` というファイル名で、SD カードのルートディレクトリに置いてください。


---
# ディスクイメージ

エミュレータでよく使用される D88 フォーマットファイルを使用します。
microSD カード上に D88 ファイルを置いておきます。

---
# 制限事項

- フロッピーのフォーマット(しれっと無視します)は未実装です。ブランクのディスクイメージはあらかじめパソコン上のエミュレータでフォーマットしておく必要があります。
- FDC の一部のコマンドしか実装していないので、FDC を直接操作するソフトは動かない可能性があります。
- FD の動作は一瞬で完了する実装になっています。タイミングが必要なソフトで不具合が出る可能性があります。
- まれに起動時のハンドシェイクに失敗します。本体をリセットしなおすと起動すると思います。

---
# ライセンスなど

このエミュレータは以下のライブラリを使用しています。

- [Z80](https://github.com/redcode/Z80/tree/master)
- [Zeta](https://github.com/redcode/Zeta)
- [no-OS-FatFS-SD-SDIO-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)

---
