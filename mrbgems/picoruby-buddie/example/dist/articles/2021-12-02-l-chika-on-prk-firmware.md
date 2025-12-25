---
layout: default
title: [PRK Firmware Advent Calendar 2021] RubyだけでLチカできちゃうんです。そう、PRK Firmwareならね（←
date: 2021-11-29 13:11:14.254339
categories:
---

この記事は[PRK Firmware Advent Calendar 2021](https://adventar.org/calendars/7086)の12月2日のエントリです。


[きのうの記事](https://shimane.monstar-lab.com/hasumin/what-is-prk-firmware)のとおり、PRK Firmwareは自作キーボードのためのファームウェアです。

しかし、以下の特徴があるため、「RubyによるIoTのフレームワーク」になることが可能です：

- keymap.rbにユーザ独自の実装を書くことができる
- そのkeymap.rbはマイコン上でオンザフライコンパイルされ、即座に実行される
- GPIOを操作するC言語の関数をラップしたRubyバインディングが読み込まれている

その証拠として、ラズパイピコにPRK Firmwareをインストールし（[その手順はこちらを参照](https://github.com/picoruby/prk_firmware#getting-started)）、keymap.rbを以下のような内容に書き換えてPRKFirmwareドライブにドラッグ＆ドロップすれば、ラズパイピコのLEDが点滅します。

```
GPIO_OUT = 1
GPIO_IN  = 0
HI = 1
LO = 0

# ラズパイピコでは、GPIOの25番が
# ボード上のLEDにつながっている
pin = 25
# そのピンを初期化
gpio_init(pin)
# そのピンを出力ピンとして設定
gpio_set_dir(pin, GPIO_OUT)
# 無限ループ
while true
  # 出力をハイに（電圧をかける）
  gpio_put(pin, HI)
  # 1秒休み
  sleep 1
  # 出力をローに（電圧をゼロにする）
  gpio_put(pin, LO)
  # 1秒休み
  sleep 1
end
```

![](/assets/images/202112/l-chika.out.png)

マイコンプログラミングにおける「Lチカ（LEDチカチカ）」というのは、パソコンプログラミングにおける「ハローワールド」に相当する「最初の一歩」です。
別の言い方をすると、マイコンにおけるGPIOは、パソコンにおける標準入出力に相当します。


標準Cライブラリの `printf()` 関数が `write()` システムコールをラップしているのと同様に、上記keymap.rbの `gpio_put()` はラズパイSDKの同名のC関数をラップし、その関数が特定のメモリアドレスに値を書き込むことでピンの電圧を操作しています。
ということは、GPIO以外の様々なペリフェラルのRubyラッパを書いておけば、PRK Firmwareは「Rubyでマイコンを操作できるフレームワーク」になります。


このことを踏まえると、マイコン上でRubyをコンパイルできることの重要性が理解されると思います。
PRK Firmwareとは単なるキーボードファームウェアではなく、コンパイルツール群を用意することなくRubyマイコンプログラミングできるフレームワークの実験場です。
私見ですが、高度に抽象化した現代のWebプログラミングなどよりも、マイコンプログラミングはコンピュータを学ぶのにもってこいです。


別言語による同種の取り組みにはMicroPyhtonなどがあり、あちらのほうがだいぶ先に進んだエコシステムを築き上げていますが、選択肢は多い方がよいでしょう。各種ライブラリの充実が当面の課題です。
というわけで、「日本発のプログラミング言語Ruby」によるマイコンプログラミングの環境整備に協力してくれる仲間を募集しています！　どこかで声をかけてください！

