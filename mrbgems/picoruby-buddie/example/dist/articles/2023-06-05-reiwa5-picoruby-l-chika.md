---
layout: default
title: 【令和5年最新版】PicoRubyとラズパイピコでLチカする方法（Lチカだけじゃないよ）
date: 2023-06-04 22:19:39.920829
categories:
---

みなさんこんにちは。
マイコンのLチカと言えば、パソコン（パソコンとは）のHello Worldのようなものです。
気軽にLチカをお試しいただけるPicoRubyじゃないと、いつまで経ってもマイクロパイソン先輩に追いつけません。
そこで今回は、気軽にLチカするチュートリアルをお届けします。

## 環境構築

下記のリンクからR2P2の最新版uf2ファイルをダウンロードして、ラズパイピコ（Raspberry Pi Pico）にインストールしてください。
インストール方法がわからない場合は、ググってください。

[https://github.com/picoruby/r2P2/releases/](https://github.com/picoruby/r2P2/releases/)

それから、お使いのパソコンにターミナルエミュレータをインストールしてください。
WindowsならTera Termがよいでしょう。
LinuxならGTKTermがおすすめです（cuやminicomは改行コード設定に難があるのでNGです）。
筆者はmacOSについてはわかりませんので、各自お調べください。

## 起動

ラズパイピコとパソコンをUSBケーブルで接続してください。
そして、ターミナルエミュレータを起動して、つながりそうなシリアルポートを適当に選んでください。
で、下のような画面が表示されたら勝利です（文字の色やフォントファミリは、環境設定に依存します）。

![](/assets/images/202306/picoruby-boot.png)

ふたつほど質問されますが、両方とも `n` と回答しておいてください（これについては後述します）。


必要に応じてターミナルの改行コードを変更してください。Tera TermならAUTOでよいでしょう。

## Lチカ！

R2P2の起動直後は、なんちゃってUnix風シェルが起動しています（これについては、もうちょっと開発が進んだら解説記事を書きます）。
きょうのところは `irb` コマンドを入力してIRBを起動してください。

（プロンプトが `$>` ではなく `irb>` になったことをしっかり確認してください！）


起動したら、以下のプログラムを1行ずつ入力してください：

```ruby
pin = GPIO.new(25, GPIO::OUT)
pin.write 1
```

![](/assets/images/202306/picoruby-l-chika.png)

いかがでしょうか？　ラズパイピコに載っているLEDが点灯したら成功です。

![](/assets/images/202306/picoruby-rp2-l-chika.jpg)

（ちなみにラズパイピコW（Raspberry Pi Pico W）のオンボードLEDはちょっと違う仕組みになっており、PicoRubyがまだ対応していません）

下のプログラムでLEDが消灯するはずです：

```ruby
pin.write 0
```

では、連続してチカチカさせましょう：

```ruby
while true
  pin.write 1
  sleep 1
  pin.write 0
  sleep 1
end
```

1秒ごとに点灯と消灯を繰り返せば大成功です。
やったね！

`Ctrl-C` を押せば無限ループが止まります。
止まらないようでしたら、R2P2のバージョンが古いかもしれません。

## Lチカだけじゃない

せっかくなので、ADCも試してみましょう。
RP2040はチップ内に温度センサを内蔵していて、ADCからその値を読むことができます。
以下にプログラムの全体を示します：

```ruby
require 'adc'

def cal_temp(val)
  27 - (val * 3.3 / (1<<12) - 0.706) / 0.001721
end

adc = ADC.new(:temperature)

while true
  puts cal_temp(adc.read)
  sleep 1
end
```

1行目の `require 'adc'` は必須です。
Lチカのときはgpio gemをrequireしませんでしたが、R2P2起動時にすでにrequire済みだったからです。

長い行はコピペするとよいです。
いちどに複数行をペーストするのは、失敗すると思います。
1行ずつペーストしてください。

![](/assets/images/202306/picoruby-adc-temperature.png)

なにやら温度っぽい値が出力されたら成功です。
RP2040チップを指であたためると、値が変わるはずです。
ちなみにこの温度センサ、ラズパイピコの出荷時点では校正されていないので、実感とだいぶ異なる値が出力されると思います。

## I2CやSPIやUARTもあります

R2P2の起動時に聞かれた質問に `y` と答えられるようなセットアップになっている人は、I2Cなどを使用する準備ができている（かもしれない）人です。
たとえばI2Cであれば、[I2C クラスの型定義](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-i2c/sig)や[PRK FirmwareのI2Cトラックボール設定例](https://github.com/picoruby/prk_firmware/wiki/Mouse#i2c)を見れば、APIがわかると思います。


現在、ペリフェラルAPIのドキュメント整備を手伝ってくれる人を募集中です。
mrubyとmruby/c（もちろんPicoRubyも含む）が、共通のペリフェラルAPI仕様を策定中です。
ので、このドキュメントに貢献するということは、mrubyワールド全体に貢献するのと同じです。


お手伝いいただくにあたって、マイコンには詳しくなくてもよいです（わたくしが適宜お教えします）が、Rubyに慣れている方だと頼もしいです。[Twitter](https://twitter.com/hasumikin)か[GitHub](https://github.com/hasumikin)でお声がけください:bow:。
よろしくお願いします～
