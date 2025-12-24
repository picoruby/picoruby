---
layout: default
title: mruby/c よもやま話
date: 2018-08-22 18:36:10.125380
categories:
---

![](/assets/images/201808/mrubyc_logo_04.png)

以前の記事 [3分で動かす ESP32 + mruby/c （もちろんマルチタスク）](http://shimane.monstar-lab.com/hasumin/esp32-mrubyc-utils) でmrubycが当時最新のmasterブランチだとESP32上でうまく動かないという報告をしました。


具体的には、実行するとすぐにrebootがかかってそれが無限ループするというものでした（エラーメッセージは何通りかあるのを確認しており、アプリ側のコードにも因るらしかったので誤りを避けるためここには書きません）。


これが、最近のコミットで動くようになりました／(^o^)＼ﾔｯﾀｾﾞ


こちらがそのコミットです [https://github.com/mrubyc/mrubyc/commit/c1a8bd44c4e4dddb3a643a4830e1de21f710ba60](https://github.com/mrubyc/mrubyc/commit/c1a8bd44c4e4dddb3a643a4830e1de21f710ba60)

----

というだけの報告ではつまらないので、もうちょっと何か書きます。


mruby/cの `vm_config.h` にいろいろな設定が書かれており、これをわれわれアプリ開発者が上書きする必要が（たまに）あります。
とくに [この行](https://github.com/mrubyc/mrubyc/blob/release1.1/src/vm_config.h#L55) の `MAX_GLOBAL_OBJECT_SIZE` がアプリの進捗に従って上限に達しやすいです。
しかもグローバルオブジェクトはmruby/c本体の開発によってもそもそも増えていくのに、このデフォルト値が増やされないためｗ、割とすぐに引っかかりますからみなさん適宜値を増やしましょう。


それと、5行下で定義されてる `MAX_CONST_COUNT` は現在使われていない模様です。
定数の数も `MAX_GLOBAL_OBJECT_SIZE` のカウントに含まれているようです。  メモリ削減のためでしょうか。
この辺はそのうちリファクタリングするとmrubycの中の人がおっしゃっていたとの噂です。


それともうひとつ。わたくしがつくっている [mrubyc-utils](https://github.com/hasumikin/mrubyc-utils) には、 `mrubyc-utils checkout -t [mrubycのコミットハッシュ値／タグ／ブランチ　など]` とすることで任意のmrubycのコミット位置に切り替える便利機能があります。


この機能の仕様のひとつに「 `vm_config.h` は上書きしない」という点があります。
普段はこのほうが便利なはずです。せっかく `MAX_GLOBAL_OBJECT_SIZE` を増やしたのにいつのまにか戻ってたというのは悲しいですから。


しかし、本家のmruby/c側で `vm_config.h` にリファクタリングが入っても `mrubyc-utils checkout -t` コマンドではそれを取得しないという問題があることになります。
ってなことだれも覚えてないでしょうけど、いちおう書いておきました！

現場からは以上です。みなさん、善きマイコン生活を！


