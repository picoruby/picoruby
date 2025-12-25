---
layout: default
title: [PRK Firmware Advent Calendar 2021] みんなで育てるPRK Firmware
date: 2021-12-07 10:26:28.604557
categories:
---

おはようございます。2021年[PRK Firmwareアドベントカレンダー](https://adventar.org/calendars/7086) 13日目の朝です。


PRK FirmwareはOSSです。多くの人々が開発に協力してくださっています。
ここで協力者のみなさまをご紹介し、PRKの初期コントリビュータとして記録しておくのはよいことだと思います。
さっそくはじめましょう。


...と思ったのですが、全員をご紹介するのはたいへんでした。
思っていたよりもPRKは多くの人々に支えられたプロジェクトであることを実感しております。
本稿ではすべての協力者をご紹介できないこと、お許しください。

## スポンサーのみなさま

github.com/picorubyでは、PicoRubyおよびPRK Firmwareの開発に[スポンサーを募っています](https://github.com/sponsors/picoruby)。そのほか、筆者個人のアカウントでもスポンサーを募っています。
スポンサーは神様ですので、まず最初にご紹介しましょう。

### [@takkanm](https://twitter.com/takkanm)さん

たっかんさんはスポンサーになっていただいただけでなく、PRKの発表直後に[prk_firmware 入門](https://zenn.dev/takkanm/articles/91c6741a4a3f26)という記事を書いて、PRKへの入門障壁を下げていただきました。
このほか、[RIGHT_SIDE_FLIPPED_SPLITというオプションのPR](https://github.com/picoruby/prk_firmware/pull/22)を出してくれたりしました。


ところで、QMKという言葉をご存じでしょうか？
「Q（急に）M（メンションが）K（来たから）」キーボードを買ってしまった、という意味です。
新しいキーボードのGB（共同購入）が始まったりすると、たっかんさんがその購入リンクに任意のアカウントへの@メンションをつけてツイッターで投げてきます。
メンションされたひとは思わずリンクを踏み、キーボードを買ってしまうというわけです。
QMK案件と呼んだりもします。

### [@kakutani](https://twitter.com/kakutani)さん

角谷さんも早くからのスポンサーです。また、[実質Ruby](https://twitter.com/search?q=%23%E5%AE%9F%E8%B3%AARuby&src=typed_query)ということばの産みの親です。

### [@igaiga555](https://twitter.com/igaiga555)さん

『[ゼロからわかる Ruby 超入門](https://gihyo.jp/book/2018/978-4-297-10123-7)』の著者である五十嵐さんもスポンサーとしてご支援くださっています。
同書はうちの神さん（非IT）も2周読んだ名著です！

### [TALPKEYBOARD](https://talpkeyboard.net/)さん

なんと、自作キーボード界の老舗ショップであるTALPKEYBOARDさんもスポンサーになっていただきました。
みなさん、自キパーツはOSSに理解のあるTalpさんで買いましょう！

## コードに直接コントリビュートしていただいたみなさま

### [@yoichiro](https://twitter.com/yoichiro)さん

Remapの作者でもあるyoichiro氏からは、日本語キーボード向けのキーコードについてコントリビュートしていただきました。
また、PRKをRemapに対応させる計画でもひきつづき会話させていただいていますし、PWMに対応してキーボードから音を鳴らそうぜ！という提案もいただいています。

### [@shugomaeda](https://twitter.com/shugomaeda)さん

世界のshugoパイセンは、デバッグプリントの改行コードについてのバグを修正してくれました。

### [@morisuteisyon](https://twitter.com/morisuteisyon)さん

小倉(愛川)氏は、PicoRubyのランタイム環境であるmruby/cのRP2040用HALを書いてくれました。たしか2019年末くらいにラズパイピコが発表され、これはRubyでキーボードを動かすチャンスなのでは？？？！！！という機運が高まりました。筆者がPicoRubyコンパイラとmruby/cとピコSDKの統合に集中しているあいだ、氏がHALを書いてくれたのでスムーズに初期開発が進みました。最大の貢献者のひとりです。

## 周辺から開発をサポートしてくれたみなさま

### [@eswai](https://twitter.com/eswai)さん

[PicoRubyマクロパッド](https://eswai.hatenablog.com/entry/2021/12/04/083643)を設計してくださったeswai氏のお陰で、[自キハンズオンが大勝利](https://shimane.monstar-lab.com/hasumin/prk-firmware-handson)でした。

### [@e3w2q](https://twitter.com/e3w2q)さん

[SU120](https://e3w2q.github.io/9/)の作者であるe3w2q氏は、「[Raspberry Pi Pico、Pro Micro RP2040を使ってPRK Firmwareに対応した自作キーボードを設計する際のポイント](https://e3w2q.github.io/19/)」という、現時点で最も有用な記事を書いてくれました。みなさんもこの記事を読んでPRK対応のキーボードを設計してみてください！

### [@swan_match](https://twitter.com/swan_match)さん

魔王さんはPRK初期からアドバイスをくださっている[RGB-LEDパイセン](https://royal-keyboard-works.square.site/)です。PRKのRGBクラスは氏なくしては成立し得ないものです。
ほかにロータリエンコーダの開発にも協力していただきましたし、UARTの相互通信（これについてはそのうち別の記事を書きます）を導入するときにグダグダした筆者の考えを聞いていただき、お陰で頭が整理されました。

### [@w_vwbw](https://twitter.com/w_vwbw)さん

はやしたろう氏もロータリエンコーダの開発を強力にサポートしてくださっていて、その成果を先日リリースしました（これについても記事を書きます）。
氏の設計する[マクロパッド群](https://tarohayashi.booth.pm/)はとても魅力的ですね。

### [@yukihiro_matz](https://twitter.com/yukihiro_matz)

まつもとさんがいなければ、PRKはもちろん、PicoRubyも存在しませんでした。いつもありがとうございます。

----

すごい。これでも全員ではないのです。
もはやPRK Firmwareはわたくしひとりのものではなくなってしまいました。
みんなで育てるPRKなのです。
