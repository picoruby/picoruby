---
layout: default
title: ESP32 + mruby/c 欲張りすぎてメモリ不足 の巻
date: 2018-08-15 18:38:46.336660
categories:
---

![](/assets/images/201808/esp32.png)

引き続きESP32（ESP-WROOM-32）とmruby/cをいじっています。


mruby/cのメモリ使用量は <del>64KB</del> 63KB （[ご指摘いただき](https://twitter.com/mrubyc_dev_jp/status/1029707704795709440?s=21)修正しました）が上限です。mrubycのソースに手を入れればこの制限は解除できそうですが、それはここでは置いといて...


ESP32はメモリがいっぱいあるからー なんて油断して下記のように最大に設定しておくと、

```
// mruby/cのVMに割り当てるメモリサイズを63キロバイトに設定
#define MEMORY_SIZE (1024 * 63)
```

make時にこういうエラーがでました ↓

```
$ make
...
LD build/my-app.elf
/home/hasumikin/esp/xtensa-esp32-elf/bin/../lib/gcc/xtensa-esp32-elf/5.2.0/../../../../xtensa-esp32-elf/bin/ld: /home/hasumi/work/my-app/firmware/build/my-app.elf section `.dram0.bss' will not fit in region `dram0_0_seg'
/home/hasumikin/esp/xtensa-esp32-elf/bin/../lib/gcc/xtensa-esp32-elf/5.2.0/../../../../xtensa-esp32-elf/bin/ld: region `dram0_0_seg' overflowed by 14432 bytes
collect2: error: ld returned 1 exit status
/home/hasumikin/esp/esp-idf/make/project.mk:410: ターゲット '/home/hasumikin/work/my-app/firmware/build/my-app.elf' のレシピで失敗しました
make: *** [/home/hasumikin/work/my-app/firmware/build/my-app.elf] エラー 1
```

いまは、WiFiやらBLEやらのライブラリを利用するアプリをつくっています。これらのライブラリもメモリをたくさん使ってしまうのですね。


エラーメッセージからわかるとおり、 `overflowed by 14432 bytes` だそうです。切り上げると15KBですから、 63 - 15 = 48 つまり

```
// 48キロバイトに減らす
#define MEMORY_SIZE (1024 * 48)
```

これでmakeできます。
大きなアプリを組もうとするとmruby/cでもこんなかんじなので、普通のmrubyはけっこう苦しいかもしれませんね。
MicroPythonだとどうなんでしょうね。


esp-idfのビルド設定を変更するなどして `not fit in region` になりにくくする方法はどんなのがあるでしょうか？
OPTIMIZATION_LEVELをDEBUGからRELEASEに変更しても、効果はちょっとだけでした。
詳しい方いらっしゃいましたら下記アカウントまで教えてくださいお願いしますｍｍ


Twitter@[hasumikin](https://twitter.com/hasumikin)
