---
layout: default
title: [PRK Firmware Advent Calendar 2021] eswaiさんのPicoRubyマクロパッド製作ハンズオン開催報告
date: 2021-12-06 10:01:00.132903
categories:
---

おはようございます。2021年「[PRK Firmwareアドベントカレンダー](https://adventar.org/calendars/7086)」6日目の朝です。
（2022年のPRKアドベントカレンダーはないので、参加するならことしがラストチャンスですよ！）


[4日目にeswaiさんが記事にしてくれたPicoRubyマクロパッド](https://eswai.hatenablog.com/entry/2021/12/04/083643)を教材として、自作キーボード製作ハンズオンの講師を務めました。
主催は、しまねソフト研究開発センター（ITOC）さんです。

![](/assets/images/202112/prk_handson_6.jpg)

一般参加定員は10名でした。お陰さまで満員になりました。
しかも全員が自作キーボードの初心者でした。
新たに何人かをキーボード沼へと引きずり込めたのではないかと思います。

![](/assets/images/202112/prk_handson_3.jpg)

見学枠で参加した1名がマイコンを反対側にハンダ付けしてしまったのを除き、全員がハンダ付けの失敗なく完成にこぎ着けることができました。

![](/assets/images/202112/prk_handson_9.jpg)

## 今回のハンズオンのよかったところ

- なんと言ってもPicoRubyマクロパッド
  - 当初はmeishi2を使用する予定でしたが、11キーのPicoRubyマクロパッドのほうが達成感が大きく、かと言って超大変でもないのがよいです
  - 見た目もよい！
- テキストエディタ（メモ帳）だけあれば参加できる
  - エディタでキーマップを書く作業を通してファームウェアの仕組みに触れてほしいが、Cコンパイラやmrubyコンパイラなどが必要だと初心者には難しい
  - それに対し、PRK Firmwareではマイコン上のPicoRubyがキーマップを動的コンパイルするので、パソコンとエディタさえあれば誰でもキーマップを更新できる

## 反省点

- ハンダ付けブースが参加者より少ないとどうしても渋滞するので、「先にハンダ付けしてあとから座学するチーム」と「あとでハンダ付けするチーム」に分けるとよさそう（スライド映写環境や講師は余計に要るが、ハンダ付けブースを増やすよりは現実的）

## トラブルシューティング

今後のため、よくあるトラブルへの対処方法をメモします：

- PRK Firmwareは、OSのキーボード設定がANSI（101/102キーボード）であることが前提です
  - JIS（106/109）など他のキーボード設定になっていると、想定と異なるキーが入力されます
  - macOSでキーボードを手動設定する方法：[https://support.apple.com/ja-jp/guide/mac-help/mchlp2886/mac](https://support.apple.com/ja-jp/guide/mac-help/mchlp2886/mac)
  - Windowsでキーボードを手動設定する方法：[https://atmarkit.itmedia.co.jp/ait/articles/1707/21/news021.html](https://atmarkit.itmedia.co.jp/ait/articles/1707/21/news021.html)

- いろいろいじっている間にラズパイピコの動作が変になってしまったら
  - [https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html#resetting-flash-memory](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html#resetting-flash-memory)
  - 上のページの「Resetting Flash memory」の項から `flash_nuke.uf2` をダウンロードして、RPI-RP2ドライブ（注意：PRKFirmwareドライブではありません。BOOTSELボタンを押しながらUSBケーブルを接続すると現れるRPI-RP2ドライブです）にドラッグ＆ドロップしてください。これによりピコが初期化されます。その後、改めて `prk_firmware_xxxxx.uf2` をドラッグ＆ドロップしてください

## 今後

同様のハンズオンを別の都市でも開きたいと考えています。ぜひやってほしい、という地方自治体・各種団体さんがいらっしゃれば[ITOC](https://www.s-itoc.jp/)さんまでご連絡ください！

![](/assets/images/202112/prk_handson_0.jpg)

（写真は参加者のご許可を得て掲載しています。マスクを外したのは集合写真を撮るほんの一瞬だけで、全員が息を止めていました）

