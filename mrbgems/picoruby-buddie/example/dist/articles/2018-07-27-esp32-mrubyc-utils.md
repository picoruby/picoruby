---
layout: default
title: 3分で動かす ESP32 + mruby/c （もちろんマルチタスク）
date: 2018-07-27 09:43:48.637486
categories:
---

![](/assets/images/201807/esp32.png)

mruby/cに[ESP32のhalがマージされた](https://github.com/mrubyc/mrubyc/pull/51)ので、mruby/c専用のテンプレ生成便利ツール「[mrubyc-utils](https://github.com/hasumikin/mrubyc-utils)」でもESP32のmruby/cアプリを生成できるようにしました。

### 必要な環境
- [esp-idf](https://github.com/espressif/esp-idf)
- mrubyコンパイラ（mrbc）
- mrubyc-utilsの[v0.0.4](https://github.com/hasumikin/mrubyc-utils/releases)以上
- python2系（mruby/cアプリをつくるのには無関係ですが、esp-idfの `make monitor` に必要です。便利です）

以上が揃っていれば、あとは3分でできます！

### やってみる

プロジェクトディレクトリをつくってそこに入ります。

```
$ mkdir esp32-mrubyc
$ cd esp32-mrubyc
```

mrubyコンパイラのバージョンを確認。

```
$ mrbc --version
mruby 1.4.1 (2018-4-27)
```

エラーになる（パスが通っていないなど）なら、（筆者の場合はrbenvをつかっているので、例えば）以下のようにしましょう。

```
$ echo 'mruby-1.4.1' > .ruby-version
```

pythonのバージョンを確認。筆者のはpyenvでインストールした2.7.14でした。

```
$ python --version
Python 2.7.14
```

エラーになる、3系が出力される、などあれば同様に（バージョン番号はお持ちのものを適切に使用してください）。

```
$ echo '2.ほげ.ほげ' > .python-version
```

ここからが本番。テンプレをインストールします。最初の質問に `esp32` 、途中はエンター空打ち、最後の質問に `yes` と答えるだけです。

```
 ~/esp32-mrubyc $ mrubyc-utils install
target microcontroller [psoc5lp/esp32/posix]: esp32     # <-ここ
dir name of mruby/c repository [.mrubyc]:
dir name where mruby/c's sources are located (main.c will include them) [components/mrubyc/mrubyc_src]:
dir name where your mruby code are lacated [mrblib]:
prefix of your mruby source file name [job]:

mruby/c and templates will be installed with configuration below:
  dir name of mruby/c repository: .mrubyc
  dir name where mruby/c's sources are located (main.c will include them): components/mrubyc/mrubyc_src
  dir name where your mruby code are lacated: mrblib
  prefix of your mruby source file name: job
Note that some file may be overwritten.
continue to install? (yes/no): yes     # <-ここ
```

----
<del>
**[暫定対処ここから]**

mrubyc-utilsはinstallする時点での最新のmruby/cのmasterをcloneします。が、ここでちょっと問題がありまして、本記事執筆時点（2018/7/27）の最新のmasterではこのあとがうまく動きません（調査中）ですので、mruby/cをすこし古いコミットまで戻します。

mruby/cの任意のコミットをチェックアウトするなんて簡単なことです。そう、mrubyc-utilsならね！

```
$ mrubyc-utils checkout -t 2c30737
```

ちなみに `-t` オプションの引数は、 `git checkout` が受け取れる値であればなんでもOKです。ここではコミットハッシュの2c30737をチェックアウトしました。

**[暫定対処ここまで]**
</del>

【2018/08/22 追記】 [解決しました](http://shimane.monstar-lab.com/hasumin/mrubyc-yomoyama-20180822)

----

ここまで来れば、ビルド（make）、インストール（flash）、ログ目視（monitor）だけです。

最初のmake実行時には、 `make menuconfig` 相当の画面になります。esp-idfのドキュメントを読んでご自身の環境に合わせた設定をしてください（筆者の場合はなにも設定するところはなく、ESCを2回押してconfig画面を閉じるだけですが）。

```
$ make
```

ファームウェアを書き込みます。IO0ボタンとENボタンを両押しして、ENを先に離す、書き込みが始まったらIO0を離す、という儀式が必要です。

```
$ make flash
```

シリアル通信でモニタリング。

```
$ make monitor
```

以上3コマンドをいっぺんに実行してもいいです。

```
$ make flash monitor
```

monitorタスクがうまく動けば、ターミナルに以下のように出力されるはずです／(^o^)＼ﾊｼﾞﾏﾀ!

```
（略。メモリなどの状況をプリントしています）
  Totals:
    free 356944 allocated 22672 min_free 356944 largest_free_block 132876
===============================================================================
1
"!"
ary[1] : 10002
2
"""
ary[2] : 10003
3
"#"
main_loop stops until sub_loop unlocks Mutex
===============================================================================
This is ESP32 chip with 2 CPU cores, WiFi/BT/BLE, silicon revision 1, 4MB external flash
Memory total:30720, used:17536, free:13184, fragment:3
check:memory usage
Heap summary for capabilities 0x00000004:
  At 0x3ffae6e0 len 6432 free 0 allocated 6304 min_free 0
    largest_free_block 0 alloc_blocks 24 free_blocks 0 total_blocks 24
（略）
```

このプログラムがなにをやっているかは、 `mrblib/` の中身や `main/main.c` をご確認ください。

あとはみなさんのコードを書くだけです。 `mrblib/` のなかの `.rb` ファイルを変更したりファイル名を変えたり新たなファイルをつくってください。

もちろん `main/main.c` も適切に編集しましょう。

ただし、mrblibのなかにディレクトリをつくって階層を変更した場合は、 `main/component.mk` の変更も必要です。Makefile力...！


