---
layout: default
title: ESP32+FreeRTOS+mruby/cでマルチタスク
date: 2018-06-29 11:06:08.703622
categories:
---

![](/assets/images/201806/FreeRTOS.png)

こんにちはマイコン刑事はすみんです。


タイトルのとおりのことができるかを実験してみました。


プロジェクトテンプレートおよびhalはこちらをベースにさせていただきました。
↓
[https://github.com/mruby-esp32/mrubyc-esp32](https://github.com/mruby-esp32/mrubyc-esp32)

### 結論を先にまとめると、
1. FreeRTOSの複数のタスクそれぞれにmruby/cのVMをひとつ＋mrubycタスクをひとつ起動→**できない**＼(^o^)／
1. FreeRTOSのひとつのタスクにmruby/cのVMをひとつ＋mruby/cタスクを複数起動（rrt0の機能を利用）→できる
1. FreeRTOSのタスクをつかわずmruby/cのVMをひとつ＋mruby/cタスクを複数起動（rrt0の機能を利用）→できる


以上でした。1.はコンパイルできますが、実行するとエラーになります。2回目の *mrbc_init();* がうまくいかずにマイコンがリブートする無限ループに突入してしまいます。
どうやらmruby/cに設計上の問題（FreeRTOSタスク上での動作を想定していない？）があるようです。そのうち直るかもしれないので気長に待ちます（他力本願）。


まあ2.と3.は動くしフットプリントはもちろん小さいので、ESP32の大きなメモリにたくさんのビジネスロジックを載せられますね。Webアプリでもつくろうかな。


mruby/cだけでつくるなら3.で、mruby/cとCの何かを並列に動かすなら2.でしょうか（ただしmruby/c側からセマフォをきちんと叩けるかは未検証です）。

### 実際のコード（抜粋）
#### 1.のmain.c抜粋（動かないやつ）
```
#include "job_main.h"
#include "job_sub.h"
#define memory_pool_for_main
#define memory_pool_for_sub
#define MEMORY_SIZE (1024*30)
static uint8_t memory_pool_for_main[MEMORY_SIZE];
static uint8_t memory_pool_for_sub[MEMORY_SIZE];

SemaphoreHandle_t xMutex;
// mrubyからセマフォ（Mutex）を呼ぶためのラッパ。この箇所は結局使えていないため未検証
static void c_xSemaphoreGive(mrb_vm *vm, mrb_value *v, int argc){
  xSemaphoreGive(xMutex);
}
static void c_xSemaphoreTake(mrb_vm *vm, mrb_value *v, int argc){
  xSemaphoreTake(xMutex, portMAX_DELAY);
}

static void task_main(void *arg) {
  mrbc_init(memory_pool_for_main, MEMORY_SIZE);
  mrbc_define_method(0, mrbc_class_object, "w_xSemaphoreGive", c_xSemaphoreGive);
  mrbc_define_method(0, mrbc_class_object, "w_xSemaphoreTake", c_xSemaphoreTake);
  mrbc_create_task(job_main, 0); //ひとつ目のmruby/cタスク
  mrbc_run();
}

static void task_sub(void *arg) {
  mrbc_init(memory_pool_for_sub, MEMORY_SIZE);
  mrbc_define_method(0, mrbc_class_object, "w_xSemaphoreGive", c_xSemaphoreGive);
  mrbc_define_method(0, mrbc_class_object, "w_xSemaphoreTake", c_xSemaphoreTake);
  mrbc_create_task(job_sub, 0); //ふたつ目のmruby/cタスク
  mrbc_run();
}

void app_main(void) {
    nvs_flash_init();
    xMutex = xSemaphoreCreateMutex();
    xTaskCreate(&task_main, "task_main", 1000, NULL, 5, NULL);
    xTaskCreate(&task_sub, "task_sub", 1000, NULL, 5, NULL);
    vTaskStartScheduler();
}
```
#### 2.のmain.c抜粋（動く）
```
#include "job_main.h"
#include "job_sub.h"
#define memory_pool
static uint8_t memory_pool[MEMORY_SIZE];

static void task_main(void *arg) {
  mrbc_init(memory_pool, MEMORY_SIZE);
  mrbc_create_task(job_main, 0); //ひとつ目のmruby/cタスク
  mrbc_create_task(job_sub, 0); //ふたつ目のmruby/cタスク
  mrbc_run();
}

void app_main(void) {
    nvs_flash_init();
    xMutex = xSemaphoreCreateMutex();
    xTaskCreate(&task_main, "task_main", 8192, NULL, 5, NULL);
}
```

#### 3.のmain.c抜粋（動く）
```
#include "job_main.h"
#include "job_sub.h"
#define memory_pool
static uint8_t memory_pool[MEMORY_SIZE];

void app_main(void) {
  mrbc_init(memory_pool, MEMORY_SIZE);
  mrbc_create_task(job_main, 0); //ひとつ目のmruby/cタスク
  mrbc_create_task(job_sub, 0); //ふたつ目のmruby/cタスク
  mrbc_run();
}
```

----

ここはこうしたほうがいいんじゃないの！　などの突っ込みがありましたらTwitter[@hasumikin](https://twitter.com/hasumikin)にお知らせいただけましたら喜びます（出雲弁）
