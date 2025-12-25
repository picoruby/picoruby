---
layout: default
title: ESP32 こんどはROM（パーティション）不足
date: 2018-08-17 14:22:39.491536
categories:
---

![](/assets/images/201808/esp32.png)

あいかわらずESP32（ESP-WROOM-32）です。


前回は、（筆者の理解が正しければ）「[ESP32のRAMを使用する想定量が限界を超えた](/hasumin/esp32-mrubyc-not-fit-in-region)」という話でした。


で今回は、ROMの件です。mruby/cは今回は関係ありません。


`make flash monitor` したら、下記のエラーがでました：

```
E (502) esp_image: Image length 1055056 doesn't fit in partition length 1048576
E (510) boot: Factory app partition is not bootable
E (516) boot: No bootable app partitions in the partition table
```

oh...


なんだべ？...

```
$ ls -l build/
...
-rw-rw-r-- 1 hasumikin hasumikin 1055056  8月 17 13:39 my-app.bin
-rwxrwxr-x 1 hasumikin hasumikin 6573624  8月 17 13:39 my-app.elf
-rw-rw-r-- 1 hasumikin hasumikin 4714491  8月 17 13:39 my-app.map
...
```

これですね。my-app.binが、ESP32のflashメモリに切られたパーティションに収まらないっていうことらしいです。


以下のように対処しました。


まず、デフォルトのパーティションテーブルpartitions_singleapp.csvを、my-appプロジェクトルートにコピーします：

```
$ cp ~/esp/esp-idf/components/partition_table/partitions_singleapp.csv ./partitions.csv
```

この中を以下のように編集します

```
# Name,   Type, SubType, Offset,  Size, Flags
# Note: if you change the phy_init or app partition offset, make sure to change the offset in Kconfig.projbuild
nvs,      data, nvs,     0x9000,  0x6000,
phy_init, data, phy,     0xf000,  0x1000,
factory,  app,  factory, 0x10000, 2M,   #←ここ！
```

たんに `1M` を `2M` に変更しただけです。これで2メガバイトまで耐えられます。たぶん。


それから、sdkconfigも編集しました。本当は `make menuconfig` で同様になるように設定すべきかもしれませんが、面倒なのでvimで。。。いずれにせよ自己責任でお願いします。

編集結果を `git diff` でみるとこうです：

```
diff --git a/sdkconfig b/sdkconfig
index 7250ed4..623fde9 100644
--- a/sdkconfig
+++ b/sdkconfig
@@ -54,11 +54,11 @@ CONFIG_ESPTOOLPY_FLASHFREQ_26M=
 CONFIG_ESPTOOLPY_FLASHFREQ_20M=
 CONFIG_ESPTOOLPY_FLASHFREQ="40m"
 CONFIG_ESPTOOLPY_FLASHSIZE_1MB=
-CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y
-CONFIG_ESPTOOLPY_FLASHSIZE_4MB=
+CONFIG_ESPTOOLPY_FLASHSIZE_2MB=
+CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
 CONFIG_ESPTOOLPY_FLASHSIZE_8MB=
 CONFIG_ESPTOOLPY_FLASHSIZE_16MB=
-CONFIG_ESPTOOLPY_FLASHSIZE="2MB"
+CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
 CONFIG_ESPTOOLPY_FLASHSIZE_DETECT=y
 CONFIG_ESPTOOLPY_BEFORE_RESET=y
 CONFIG_ESPTOOLPY_BEFORE_NORESET=
@@ -89,11 +89,11 @@ CONFIG_MAX_STA_CONN=4
 #
 # Partition Table
 #
-CONFIG_PARTITION_TABLE_SINGLE_APP=y
+CONFIG_PARTITION_TABLE_SINGLE_APP=
 CONFIG_PARTITION_TABLE_TWO_OTA=
-CONFIG_PARTITION_TABLE_CUSTOM=
+CONFIG_PARTITION_TABLE_CUSTOM=y
 CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
-CONFIG_PARTITION_TABLE_FILENAME="partitions_singleapp.csv"
+CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
 CONFIG_PARTITION_TABLE_OFFSET=0x8000
 CONFIG_PARTITION_TABLE_MD5=y
```

筆者の場合、ESP32には4メガバイトあるはず（ですよね？）のflashのうち2メガバイトしか使わない設定になってました。ですので4メガ使うようにしました。そうじゃなきゃapp用に2Mのパーティションは切れませんので。
それと、partitions.csvを参照するような設定にしました。


これで無事に `make flash` できました。やったね！


