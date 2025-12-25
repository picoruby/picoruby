---
layout: default
title: mruby/c（POSIX）で正確な時間計測
date: 2019-01-05 23:18:07.292458
categories:
---

あけましておめでとうございます。


お正月と言えば、傘の上で鞠や熊のプーさんがくるくる転がりますね。
そんなわけなので、mruby/cのループ速度を計測してみました。比較のためCRubyでもやってみました。
ただひたすらに空っぽのループを回して、1ミリ秒あたり何回まわせるのかを計算します。

### ソース

```
# job_loop.rb

if RUBY_VERSION.to_f < 2.1 && MRUBYC_VERSION
  # mruby/cの場合(RUBY_VERSION == "1.9" なのを条件分岐に利用)
  # get_monotonic_arrayメソッドはmain.cに実装されています
  puts 'MRUBYC_VERSION: ' + MRUBYC_VERSION
  class String
    # mruby/cにはrjustが（ljustも）ないので、なんとなく実装します
    def rjust(length, padstr = ' ')
      if self.length < length
        padstr * (length - self.length) + self
      else
        self
      end
    end
  end
else
  # CRubyの場合（バージョン2.1未満では動きません）
  puts 'RUBY_DESCRIPTION: ' + RUBY_DESCRIPTION
  def get_monotonic_array
    # mruby/c版がC言語のclock_gettime()関数を使うので、こちらも同等（たぶん）の実装をしてみます
    nsec_str = Process.clock_gettime(Process::CLOCK_MONOTONIC_RAW, :nanosecond).to_s
    [nsec_str[0..(nsec_str.size - 10)].to_i, nsec_str[(nsec_str.size - 9)..-1].to_i]
  end
end

def get_literal_duration(start, finish)
  start_sec = start[0]
  finish_sec = finish[0]
  start_nsec = start[1]
  finish_nsec = finish[1]
  duration_nsec = if start_nsec > finish_nsec
    # 引き算の繰り下がり
    finish_sec -= 1
    finish_nsec - start_nsec + 1_000_000_000
  else
    finish_nsec - start_nsec
  end
  (finish_sec - start_sec).to_s + '.' + duration_nsec.to_s.rjust(9, '0')
end

def benchmark
  start = get_monotonic_array
  yield
  finish = get_monotonic_array
  puts '  start:  ' + start.to_s + '(sec, nsec)'
  puts '  finish: ' + finish.to_s + '(sec, nsec)'
  return [start, finish]
end

def print_result(res_array, count)
  literal_duration = get_literal_duration(res_array[0], res_array[1])
  puts '  literal duration: ' + literal_duration
  duration = literal_duration.to_f
  puts '  FLOATed duration: ' + duration.to_s
  puts '  ' + (count / duration / 1000).to_s + " loops per 1 millisec"
  puts
end

# この数だけループを回して計測します
#LOOP = 5_000_000
LOOP = 500_000
puts
puts "ループ回数: " + LOOP.to_s
puts

# for inループ
puts "for inループ"
res_array = benchmark do
  for i in 1..LOOP do
    # do nothing
  end
end
print_result(res_array, LOOP)

# timesループ
puts "timesループ"
res_array = benchmark do
  LOOP.times do
    # do nothing
  end
end
print_result(res_array, LOOP)

# whileループ
puts "whileループ"
i = 0
res_array = benchmark do
  while i < LOOP do
    i += 1
  end
end
print_result(res_array, LOOP)
```

job_loop.rbの大部分はCRubyとmruby/cで共通のスクリプトにするための小細工なのですが、あーでもないこーでもないと書いていたらこっちの方が解説の必要な感じになってしまいました。（プログラムが下手なだけ。。。じつはmruby/cのバグっぽい挙動のせいで変な実装にならざるを得なかった箇所もあります。そこは追々...）


で今回、時間を計測するのにC言語の `clock_gettime()` 関数が返す値をなるべくきちんと使おうとして若干めんどうな実装になりました。
秒セクションとナノ秒セクションを整数のまま別々に計算してからくっつけることでFloat化による誤差を減らそうとしています。
OSが起動してからの時間が長いと誤差が増えるというのが気に入らなかったのです。詳しくはググってください。


mruby/cは32bitプロセッサのマイコン向けにつくられているので、Fixnumが32bitなんですね。符号つき4バイト整数ってやつです。int32_t。
表現できる値の範囲は `-2,147,483,647` から `2,147,483,647` だと思います。

これも秒セクションとナノ秒セクションを別々に計算した理由のひとつです。 `1,234.123456789` 秒をナノ秒単位で `1,234,123,456,789` ナノ秒と表現したくても、32bitに収まりません。


結局は最後にミリ秒毎のループ回数計算でFloatの割り算をしており、その誤差を考慮したらここでこんなに頑張る必要はないでしょうけど、なんとなく頑張ってみました。
いつか別の場所で役立つかもしれませんし。

ソースのコメントを併せてお読みください。


それと、mruby/cにはmain.cも必要です：

```
#include "mrubyc_src/mrubyc.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "src/job_loop.c"
#define MEMORY_SIZE (1024*40)
static uint8_t my_memory_pool[MEMORY_SIZE];

/*
  時間計測メソッド。単位はナノ秒
*/
static void c_get_monotonic_array(mrb_vm *vm, mrb_value *v, int argc){
  struct timespec ts;
  mrb_value mrb_array = mrbc_array_new(vm, 2);
  clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
  mrb_value sec = mrb_fixnum_value(ts.tv_sec);
  mrb_value nsec = mrb_fixnum_value(ts.tv_nsec);
  mrbc_array_set(&mrb_array, 0, &sec);
  mrbc_array_set(&mrb_array, 1, &nsec);
  SET_RETURN(mrb_array);
}

int main(void) {
  mrbc_init(my_memory_pool, MEMORY_SIZE);
  mrbc_define_method(0, mrbc_class_object, "get_monotonic_array", c_get_monotonic_array);
  mrbc_create_task(job_loop, 0);
  mrbc_run();
  return 0;
}
```

main.cとjob_loop.rbを統合する方法がわからない場合は、↓の記事を参考にしてみてください。

[mruby/cを普通のパソコンで動かす](http://shimane.monstar-lab.com/hasumin/tutorial-mrubyc-utils-2)


ディレクトリの中身はこんな感じです：

```
~/work/loop $ tree -L 2
.
├── .gitignore
├── .mrubyc
│   ├── .git
│   （略）
├── .mrubycconfig
├── .ruby-version          # 'mruby-1.4.1'にしてあります
├── Makefile
├── main
├── main.c
├── mrblib
│   ├── .ruby-version     # '2.6.0'にしてあります
│   └── job_loop.rb
├── mrubyc_mrblib
│   ├── Makefile
│   （略）
├── mrubyc_src
│   ├── Doxyfile
│   （略）
└── src
    └── job_loop.c
```

### 結果

では結果を見てみましょう。

#### CRubyの場合

```
~/work/loop/mrblib $ ruby job_loop.rb
RUBY_DESCRIPTION: ruby 2.6.0p0 (2018-12-25 revision 66547) [x86_64-linux]

ループ回数: 5000000

for inループ
  start:  [22509, 394761284](sec, nsec)
  finish: [22509, 877712311](sec, nsec)
  literal duration: 0.482951027
  FLOATed duration: 0.482951027
  10353.016600997933 loops per 1 millisec

timesループ
  start:  [22509, 883018928](sec, nsec)
  finish: [22510, 263524838](sec, nsec)
  literal duration: 0.380505910
  FLOATed duration: 0.38050591
  13140.400368551436 loops per 1 millisec

whileループ
  start:  [22510, 268057887](sec, nsec)
  finish: [22510, 627531868](sec, nsec)
  literal duration: 0.359473981
  FLOATed duration: 0.359473981
  13909.212527957621 loops per 1 millisec
```

#### mruby/cの場合

```
~/work/loop $ make && ./main
（ビルド中にwarningが出ますけどとりあえずスルー）
MRUBYC_VERSION: 1.2

ループ回数: 5000000

for inループ
  start:  [22150, 39382478](sec, nsec)
  finish: [22156, 436361768](sec, nsec)
  literal duration: 6.396979290
  FLOATed duration: 6.39698
  781.619 loops per 1 millisec

timesループ
  start:  [22156, 436836424](sec, nsec)
  finish: [22162, 212736254](sec, nsec)
  literal duration: 5.775899830
  FLOATed duration: 5.7759
  865.666 loops per 1 millisec

whileループ
  start:  [22162, 212934679](sec, nsec)
  finish: [22164, 283402948](sec, nsec)
  literal duration: 2.070468269
  FLOATed duration: 2.07047
  2414.91 loops per 1 millisec
```

予想どおり、mruby/cはCRubyよりもだいぶ遅いです。
CRuby、mruby/cともに `for in` < `times` < `while` の順で早くなっていますね。

とくにmruby/cの `for in` と `times` が `while` より3倍遅いのが目立ちます。
Range#eachとFixnum#timesがRubyで実装されているからです。

このなかを見ればわかります：

[https://github.com/mrubyc/mrubyc/tree/master/mrblib](https://github.com/mrubyc/mrubyc/tree/master/mrblib)

速度を求めないmruby/cの開発方針ってことですね。


実行するたびに値は前後しますし、ループ回数を変えると違う結果になるかもしれません。みなさんも試してみてください。


以上、いつもより余計に回してみました！（←これを言いたかっただけ）

### 実験環境
- マシン：Surface Pro（第5世代） / インテルCore i7
- OS：VirtualBox上にインストールしたLinux Mint 18.2

```
~ $ cat /proc/cpuinfo
（略）
model name      : Intel(R) Core(TM) i7-7660U CPU @ 2.50GHz
stepping        : 9
cpu MHz         : 2496.084
cache size      : 4096 KB
（略）

~ $ cat /proc/version
Linux version 4.15.0-39-generic (buildd@lcy01-amd64-012) (gcc version 5.4.0 20160609 (Ubuntu 5.4.0-6ubuntu1~16.04.10)) #42~16.04.1-Ubuntu SMP Wed Oct 24 17:09:54 UTC 2018
```


