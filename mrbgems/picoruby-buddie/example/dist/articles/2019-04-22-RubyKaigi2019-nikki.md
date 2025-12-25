---
layout: default
title: RubyKaigi2019に参加してきました
date: 2019-04-22 19:37:36.290049
categories:
---

ひとつ前の記事が登壇ノウハウな内容になってしまったので、日記は日記としてこちらに書きます。

## 印象に残ったトーク

### How to take over a Ruby gem

[https://rubykaigi.org/2019/presentations/maciejmensfeld.html#apr18](https://rubykaigi.org/2019/presentations/maciejmensfeld.html#apr18)

RubyKaigi2019の直前に、RubyGems.orgのアカウント乗っ取りによってgemにマルウェアが仕込まれるという事件があったのですが、それを予見していたかのような発表内容です。単に穴があるよ、というだけではなく、対策とセットの提案をもってくるというのが素晴らしいです。

Rubyのエコシステムが信頼できるものであり続けるのは極めて重要なことですね。

### Pragmatic Monadic Programing in Ruby

[https://rubykaigi.org/2019/presentations/joker1007.html#apr18](https://rubykaigi.org/2019/presentations/joker1007.html#apr18)

ジョーカーさんという、この世界で有名なTracePointマニアがいます。
わたくしハスミのことしの発表内容の一部にもTracePointを使っているのですが、これはRubyKaigi2018でのモリスさん＋ジョーカーさんの発表からインスピレーションを受けたものなのでなんというか恩義がありますｗ
　👇こちら
[https://rubykaigi.org/2018/presentations/joker1007.html#may31](https://rubykaigi.org/2018/presentations/joker1007.html#may31)


で、ジョーカーさんはことしはRuby2.6に導入された `RubyVM::AbstractSyntaxTree` を軸に発表をしてくれました。なんというかエッジ機能芸人とでも言えるような風格をまとっている人です。

### Building a game for the Nintendo Switch using Ruby

[https://rubykaigi.org/2019/presentations/amirrajan.html#apr19](https://rubykaigi.org/2019/presentations/amirrajan.html#apr19)

ニンテンドーSwitch（しかも市販品）の上でmruby製のゲームを動かすという話でした。まったくクレイジーなひとがいるものですね（もちろん褒めています）。

### The Selfish Programmer

[https://rubykaigi.org/2019/presentations/searls.html#apr20](https://rubykaigi.org/2019/presentations/searls.html#apr20)

これが今回いちばん印象にのこりました。スライドの作りこみ、話の展開など、スーパーワンダフルでした。
ここでぼくが内容を解説してもまったくあの価値を毀損するだけだと思うので、ビデオの公開を正座して待ちましょう。

### Optimization Techniques Used by the Benchmark Winners

[https://rubykaigi.org/2019/presentations/jeremyevans0.html#apr20](https://rubykaigi.org/2019/presentations/jeremyevans0.html#apr20)

これもすごかったです（語彙）。
すでに広く使われているgemが存在するマーケットに、用途は同一でも「もっと高速」という価値で選択肢を増やしてくる作者です。 `Sequel` や `Roda` あたりの解説でした。
そのテクニックを70分しゃべり続けて聴衆を完全に黙らせましたね。

----

ほかにも観たかったプレゼンがたくさんありまして、RubyKaigiの密度の濃さがガツンガツンと感じられる、そんな三日間でした。現場からは以上です！

