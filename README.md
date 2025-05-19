# capture_from_drm
## 概要
libdrmを用いた画面キャプチャの例です。\
詳細は以下の記事を参照ください。

動作確認はRaspberry Pi 4BとRaspberry Pi zero 2 Wで行っています。
（未確認ですが、drmが組み込まれたカーネルであれば、ハードウェアによらず動くはずです。その際は、開くデバイスパスに注意してください）

## 環境
- g++
- make
- pkg-config
- libdrm-dev
  - `sudo apt install g++ make pkg-config libdrm-dev`

## build＆実行
`make && sudo ./capture_drm_sample`
  - 実行ユーザを`video`グループへ追加すると`sudo`なしで実行可能
