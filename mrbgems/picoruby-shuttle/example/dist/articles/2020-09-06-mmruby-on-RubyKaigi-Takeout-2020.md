---
layout: default
title: RubyKaigi Takeout 2020で発表した「mmruby」について
date: 2020-09-06 10:22:20.365099
categories:
---

2020年9月4日から5日に、[RubyKaigi Takeout 2020](https://rubykaigi.org/2020-takeout)がオンラインで開催されました。

ぼくらの仲間によるナイスなまとめ記事があるので、まずはお読みください。


日本語の記事：
[RubyKaigi Takeout 2020 Day1 参加レポート](https://shimane.monstar-lab.com/okuoku/rubykaigitakeout2020-day1)
[RubyKaigi Takeout 2020 Day2 参加レポート](https://shimane.monstar-lab.com/okuoku/rubykaigitakeout2020-day2)


英語の記事：
[RubyKaigi Takeout 2020: Day One](https://engineering.monstar-lab.com/2020/09/04/RubyKaigi-Takeout-2020-Day-first)

## 登壇しました

で、わたくしも登壇させていただきました。
しかし、（完全に言い訳ですけど）準備時間が足りなくて不完全燃焼に終わりました。


英語でしゃべったということもあり、細かい情報を伝えられなかったとも思います。
そこでこの記事では、発表した内容を日本語で補足します。ちょっと長くなるけどお付き合いください。


発表スライドは[こちら](https://slide.rabbit-shocker.org/authors/hasumikin/RubyKaigiTakeout2020/)。


プレゼン録画はこちら（そのうちリンクします）。

## そもそも何をつくったか（つくっているか）

「mmruby」という名の新たなRuby言語処理系です。
「mini mruby」という意味です。


リポジトリ：
[https://github.com/hasumikin/mmruby](https://github.com/hasumikin/mmruby)


Linux上でプロジェクトをmakeすると、3つのバイナリ「mmrbc」「mmruby」「mmirb」がつくられます。

### mmrbc

「mini mruby compiler」の意味で、Rubyスクリプトをmrubyバーチャルマシン用のVMコードへコンパイルします。
本プロジェクトの中核に位置する実装部品です。

```
echo 'puts "Hello World!"' > hello.rb
./build/host-debug/bin/mmrbc hello.rb
```

上記のようにすると、 `hello.mrb` というVMコードファイルが生成され、このVMコードはmruby VMとmruby/c VMのどちらでも動作します。

### mmruby

コンパイラとしてのmmrbc、バーチャルマシンとしてのmruby/cを組み合わせ、Rubyスクリプトを直接に実行できるインタプリタ実装です。
下の絵はDEBUGビルドしたmmrubyをLinux上で走らせた様子です。

`puts "Hello World!"` というスクリプトがトークナイズされ、構文木になり、VMコードへ変換され、ついに実行される様子がわかります。

![](https://raw.githubusercontent.com/hasumikin/mmruby/master/docs/images/debug-print.png)

mmrubyの具体的な動かし方は後述します。

### mmirb

名前からわかるとおり、mmrubyのシェル（REPLと言うべき？）です。
興味のある方は実装（[ここ](https://github.com/hasumikin/mmruby/blob/master/cli/mmirb.c)と[ここ](https://github.com/hasumikin/mmruby/blob/master/bin/mmirb.rb)。あとMakefileのirbターゲット）を見ていただきたいですが、ふつうのREPLと異なり、プロセスをforkしてシグナル連携しています。


クライアント（シェルの入力側）にはさらに別のプロセスにターミナルエミュレータを起動することが想定されています。
合計3つのプロセスを使用してmmirbという機能を動作させているわけです。


なぜこんなに面倒なことをしているのでしょうか？
mmrubyはワンチップマイコン上で動作することを前提にしており、その動作モデルをLinux上で再現しているのがmmirbだからです。


マイコンにはLinux（というかPOSIX）のような標準入出力がありません。
POSIXでいうところのselectシステムコールのような仕組みはない、と言い換えることもできます。
マイコン上にシェル的なものをつくるには、UARTなどのシリアル通信を用いて端末と接続し、文字が入力されたことをハードウェア割り込み機能によってmmruby側へ通知してあげる必要があります。


マイコンでのmmrubyの動作を模式化すると以下のようになります：

```
[端末] # パソコンのキーボードで文字を入力する
  ↓
[シリアルケーブル]
  ↓
[ [UART-->割り込み]-->[mmrubyプロセス-->UART] ] # マイコン
  ↓
[シリアルケーブル]
  ↓
[端末] # パソコンのモニタに結果が表示される
```

POSIXならこれらのことが一発で済む（乱暴な言い方ですが）のに対し、マイコンではもうちょっと手間がかかりますし、移植性を確保するための抽象化を考慮する必要性があります。
Linux上でmmirbを動かしたときの3つのプロセスは、上の模式図内の[端末]、[UART-->割り込み]、[mmrubyプロセス-->UART]にそれぞれ相当し、「マイコンの面倒さ」をあえてLinux上に再現しています。
[シリアルケーブル]の部分を担うのはsocatによるファイルディスクリプタ間のリレーです（後述）。

すべてはマイコン向け実装のための実験なのです。
mmirbの動かし方も後述します。

## mruby machine

RubyKaigiでの発表タイトルの「mruby machine: An Operating System for Microcontroller」の意味は、マイコン上でmmrubyプロセスが動き、基礎リソース（CPUとRAM）とペリフェラル（UARTやLEDやADCなどなど）全体をRuby言語によってコントロールできるシステム、ということです。

このコンセプトを十全に体現した実装はまだできておらず、Takeoutの発表としては中途半端感の否めないものとなってしまいました。

## mmrbcの必要性

プレゼン中では若干の受け狙いもあり、マイクロパイソンパイセンを引き合いに出してmmrbc（ミニmrubyコンパイラ）の必要性を説きました。
しかし実は、MicroPythonは後付けの理由に過ぎません。


「小リソースマイコン向けのmrubyコンパイラ」が存在しないことは、ずーーーーーっと以前からの個人的な懸案でした。

### mrubyのRAM消費

mrubyは一般的に、400KB程度以上のRAMで動作する、と言われています。
近年、この消費量を削減する努力が傾けられ、300KBくらいあればそこそこのmrubyアプリケーションが動作するようになってきているようです。
コア実装を最低限に保つためにライブラリ化できるものを外に追い出したり、構造体の設計を見直したり、という努力です。


しかし残念ながら、この努力は（ワンチップマイコン組み込み用途の観点では）、コストパフォーマンスが悪いのです。
400KBが300KBに減ったところで、mrubyの動作する環境が劇的に増えるわけではありません。
マイコンのRAM容量ラインナップは一般的に、64KBの上は128KB、その上は256KB、その上は512KBっていう感じでしょう。
ですから、マイコン界におけるmrubyの版図を広げるためには、「256KBで楽々動く」あるいはさらに「128KBでもそこそこ実用的」まで行かなければなりません。


「mrubyはRubyのISO規格に概ね適合する」（[https://github.com/mruby/mruby#what-is-mruby](https://github.com/mruby/mruby#what-is-mruby)）という十字架が、現状以上のメモリ削減を難しいものにしています。おそらく。

### mrubyでいいじゃん？

昨今ではRAMが1MBあるようなマイコンも増えているのだから、いまのmrubyで十分じゃん、と思われるかもしれません。
しかしRAMが大きければそれだけ電力消費が増えます。
リソースの大きなマイコンはついでに（？）WiFiやらBLEやらいろいろなモジュールが付いていることも多く、さらに電力を消費するし、価格も上がります。

リソースの小さなマイコンも選択肢に含められるのは、現実のデバイス開発においては無視できないメリットなのです。

### そこでmruby/cですよ

小リソース向けのmruby実装である [mruby/c](https://github.com/mrubyc/mrubyc) はISOに準拠する気がさらさらなくて、


「♪Module無ェ！　♪eval無ェ！　♪TracePoint?あるわけ無ェ！！」


という限界集落実装です。
ではオラこんな村イヤだって思うかというと、意外とそうでもありません。


mruby/cが対応する範囲のRuby機能でも十分にルビーなんです。
String、Array、HashがあるだけでこんなにIoT開発がハッピーになるのか！
独自クラスでハードウェアをオブジェクト化すると、こりゃまじでエムルビーマシンだぜ！って思いますよ。
ぜひやってみてください。


だいいち、マイコンでevalが動いたらIoTセキュリティ委員会（誰）に怒られそうですし、TracePointがないならGDBで動作を追いかければいいのです（？）。


まあTracePointはmrubyにもありませんけど。

### mruby/cの問題点

たったひとつです。
「釣り合うサイズ」のコンパイラがないことです。
mruby/cはVMだけの実装であり、VMコードはmrbcでつくる必要があります。


mrbc（mruby-compiler）はmruby本体の実装に依存しており、mrubyから切り離しては使えません。
したがって、mruby/cのためのマイコン上コンパイラとしてmrbcを使うのはdosen't make senseです。
だってmrubyコンパイラにmrubyが付いてくるなら、mruby/cを使う必要ないですもんね。
（厳密にはVMのコードだけ剥がしてどうのこうのできる可能性はある？...いやいやいや面倒すぎます）


そして何度も繰り返すようで恐縮ですが、mrubyだとワンチップマイコンには載らないのです。


というわけでmmrbcをつくりはじめたのでありました。

### ミニ・エムルビー・コンパイラ

実装上の特徴は、パーサジェネレータに [Lemon](https://sqlite.org/src/doc/trunk/doc/lemon.html) を使用している点です。
ふつうはYacc/Bisonを使いますが、Lemonを使いました。
これについてはRubyKaigiのプレゼン内で詳しく説明してあります。


が、RubyKaigiではうまく伝えられなかったと思う点があります。
「Lemonを使えば、Bisonよりもメモリ消費の小さなコンパイラがつくれるらしい」というようなことをプレゼン内で発言しましたが、ここでいう「小さい」はコードサイズつまりROM消費量のことです。

RAM消費量の多寡は、構文木の構造体などの設計によるところが支配的なはずなので、LemonだからといってRAM消費量を減らせるわけではないと考えています。

## やってみよう

せっかくなのでチュートリアルも書きます。
えらく長い記事になったけど分割しません！

### ターミナルでmmirbを試す。

このセクションはLinux、macOS、Windows上のWSLやCygwinなど多くの環境で動くと思います。


あらかじめrbenvでCRubyとmrubyをインストールしてください。
ビルドに必要です。
インストールすべきバージョンは、エラーメッセージで確認してくださいｗ（ここに具体的なバージョン番号を書いても、そのうち変わってしまうと思うので）。

```
git clone --recursive https://github.com/hasumikin/mmruby.git
cd mmruby
make
```

これで、ホストマシン用の実行ファイルができあがるはずです。
エラーが出たら、メッセージを読んで必要なものをインストールしてください。
git cloneするときに `--recursive` を忘れずに。mruby/cがサブモジュールになっています。


#### まずはmmruby

```
./build/host-debug/bin/mmruby -e 'puts "Hello World!"'
```

とすれば、上のほうで紹介したようにデバッグプリント付きでRubyが動くはずです。


下のように、ファイルを読み込んで実行することもできます。

```
echo 'puts "Hello World!"' > hello.rb
./build/host-debug/bin/mmruby hello.rb
```

#### mmirb

さてmmirbです。
これを動かすのには、socat（ソケットリレーツール）とcu（ターミナルエミュレータツール）が必要です。
それぞれの環境で適切にインストールしておいてください。


用意ができたら、下記コマンドを入力してください（キャプチャの(1)）。

```
make irb
```

うまく動いたら、こんなメッセージが出る（キャプチャの(2)）ので、そのコマンドを別のターミナルに打ち込んでください（キャプチャの(3)）。
どうですか？
irbっぽいサムシングが動きましたか？？？（キャプチャの(4)）

![](/assets/images/202009/make-irb.png)

macでうまくいかないようでしたら、 [bin/mmirb.rb](https://github.com/hasumikin/mmruby/blob/master/bin/mmirb.rb) を直してプルリクください！

### PSoC5LPで動かす

ここから先は、Windows限定です。
PSoC CreatorというIDEがWindows版しかないからです。
より新しいPSoC6マイコンならばクロスプラットフォームなIDEがあるのでLinuxやmacでも開発できるのですが、今回はPSoC5LP（これがmruby/cのリファレンスボード）なので。。。
mac向けはまあそのうち。


手順は、すでにリポジトリのREADMEに詳しく書いてありますからそちらをみてください：
[https://github.com/hasumikin/mruby_machine_PSoC5LP](https://github.com/hasumikin/mruby_machine_PSoC5LP)

## ロードマップ

mmrbcはまだ完成しておらず、必要な文法の半分くらいにしか対応できていません。


文法の対応状況と今後の予定はこちら：
[https://github.com/hasumikin/mmruby/issues/6](https://github.com/hasumikin/mmruby/issues/6)


シェルの実装も最低限しかないので、これでは使いにくいことも自覚しております。


ところで、前述のとおりmruby/cにはevalがないですけど、コンパイラが同梱されるのなら技術的には可能かもしれませんね。
マイコンにeval要るか？っていうと要らない気はしますが、動いたら単に嬉しいのでそのうちつくるかもしれません。

## まとめ

ここまで読んだ人（いないと思うけど）ありがとうございました。


シェルの実装をもっと充実させて、これを技術的に深掘りすると面白い話になると思うので、来年のRubyKaigiまたは別のどこかで発表できたらいいなーと思っています。


ではまた(・∀・)ﾉｼ
