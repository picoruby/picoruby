---
layout: default
title: 2024年、PicoRubyの圧倒的な進捗まとめ
date:   2024-12-24 08:00:00
categories:
---

この記事は、[mrubyファミリ Advent Calendar 2024](https://qiita.com/advent-calendar/2024/mruby-family)の24日目の記事です。

----

ことしはPicoRubyに圧倒的な進捗がでたので、まとめます。

## 新mrubyコンパイラ

ユニバーサルパーサであるPrismを利用してコンパイラを刷新。マイコン上での省メモリなコンパイルとCRuby互換のシンタックスを実現。

成果物：[https://github.com/picoruby/mruby-compiler2](https://github.com/picoruby/mruby-compiler2)

## マイコンペリフェラルライブラリ

mrubyファミリ共通 I/O APIガイドライン[^1]に準拠した GPIO, ADC, I2C, SPI, UART, PWM のRP2040向けライブラリを整備。

成果物：[https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-gpio](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-gpio) など

[^1]: [https://github.com/mruby/microcontroller-peripheral-interface-guide](https://github.com/mruby/microcontroller-peripheral-interface-guide)

## 標準ライブラリ

PicoRubyをよりRubyらしく使うために、標準ライブラリを充実（一部2023年に実装したものを含みます）。

- Base 16, Base64
- MbedTLS を使用したハッシュ関数や暗号関連機能
- SQLite3（組み込みデータベース）
- Dataクラス（イミュータブルな構造体）
- JSON、YAML、JWT の各クラス (IoT フレームワークとしての実用性を向上）
- Netクラス（HTTPS クライアントによるTLS通信。@s01氏ありがとう）
- BLEクラス（CentralとPeripheral両方をPicoRubyで構成）
- Picoline（Highline風の対話式設定構成ツール）
- Picotest（Minitest風のユニットテストフレームワーク）
- POSIX用IO/File, Dirの各クラス（mruby-ioとmruby-dirから移植）

成果物：[https://github.com/picoruby/picoruby/tree/master/mrbgems](https://github.com/picoruby/picoruby/tree/master/mrbgems) この中にたくさんある


## メタプログラミング機能

- Object#methods
- Object#instance_variables, instance_variable_get, instance_variable_set
- Object#instance_eval
- Object#respond_to?
- Object#method_missing
- Kernel.#caller
- Kernel.#eval

...など（一部mruby/cへマージしたものを含みます）

成果物：（略）

## Wasmへのポーティング

PicoRuby.wasmをつくりました。
これについてはあしたの記事に改めて書きます。

## ドキュメンテーション

RBSファイルのシグネチャ情報を収集して自動生成されるAPIドキュメントを開発。

成果物：[https://picoruby.org/](https://picoruby.org/)

----

以上のような感じです。
開発しすぎたので忘れてることもありそうです。
あしたはアドベントカレンダー最終日。
巷で噂のPicoRuby.wasmについて書きます！

----
