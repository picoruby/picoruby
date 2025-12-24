---
layout: default
title: irb.wasmの入力メソッドをSTDIOからRelineへ改修しました
date: 2022-12-07 17:52:00.984548
categories:
---

これは[Ruby Advent Calendar 2022](https://qiita.com/advent-calendar/2022/ruby)、24日目の記事です。
きのうの記事は@kozo2さんの「[RubyでQuartoを活用する](https://qiita.com/kozo2/items/229af214dbc9662e4749)」でした。

----

@kateinoigakukunさんが実装した[ruby.wasm](https://github.com/ruby/ruby.wasm)は、あした（今夜？）リリースされるはずのRuby 3.2における目玉フィーチャのひとつです。
ことしのRubyアドベントカレンダーにもwasmの話題がいくつかありますね。


この記事では、irb.wasmの入力メソッドをRelineへとアップグレードしたことについて書きます。


この記事の公開時点では、下記のURLにて利用できます。将来Reline版がデフォルトに格上げされたら、URLは変更されると思います：

- 旧来のSTDOUT版irb.wasm→ [https://irb-wasm.vercel.app/](https://irb-wasm.vercel.app)
- 新しいReline版irb.wasm→ [https://irb-wasm.vercel.app/?FEATURE_XTERM_RELINE=1](https://irb-wasm.vercel.app/?FEATURE_XTERM_RELINE=1)

## きっかけ

11月のRubyWorld Conference 2022にて、PicoRubyのシェル環境について発表しました（参考：[RubyWorld Conference 2022に登壇しました](https://shimane.monstar-lab.com/hasumin/RubyWorld-Conference-2022)）。
これにはピュアPicoRubyで書かれたラインエディタおよびスクリーンエディタの実装が含まれていました。
ワンチップマイコン上で動作する、マルチライン履歴を呼び出せるシェルや、なんちゃってVi風エディタをデモしました。


これはCRubyでいうところの、Relineに相当する実装（もちろん機能はずっと貧弱）です。このデモを見た@mameさんが、もしかしたらirb.wasmに生かせるのではないか、と声をかけてくれました。

## IRBの入力メソッド

IRBには3つの入力メソッドが用意されています。最もシンプルなSTDIO、C拡張を利用するReadline、そしてRelineです。
Ruby2.7以降では、Readlineに代わってRelineが標準の入力メソッドになりました。


11月の時点で公開されていたirb.wasmは、STDIOの入力メソッドを利用していました。
その理由は、Relineが端末にcookedモードを期待しているのに対して、Webブラウザ上のターミナルエミュレータ（後述）がrawモードで動作するからです。

## cookedモードとrawモード

通常、Unixライクな端末はcookedモードで対話を開始します（要出典）。
cookedモードは、低レベルのキー入力をバッファリングして、必要に応じてプリプロセスして、「エンター」が押されたときにデータをアプリケーションへ送信します。


他方、rawモードはキーボードのすべての入力をアプリケーションへ逐一送信します。


また、termiosライブラリを使用することによって、シグナルを生成する制御文字の扱いを変更するなど、「完全なrawモード」と「典型的なcookedモード」の中間的な振る舞い（cbreadモード）にすることが可能です。

実際のPOSIXアプリケーションでは、完全なrawモードにしてしまうとアプリケーションに発生した異常のために一切の操作が利かなくなる恐れがあるため、なんらかの中間的なモードを設定して、たとえばCtrl-Zで強制終了できるようにプログラムすることが多いと思います（要出典）。

## ちょっと脱線

ここでRubyのIOメソッドを、rawモードとcookedモードにフォーカスしていくつか見てみましょう。


（文脈に沿ったピックアップであり、引数の説明などを適宜端折りますから、網羅性、正確性はありません）

### 組み込みIOクラスのメソッド

#### IO#gets
入力を1行読み込みます。わかりやすいcookedモードですね。
`puts` の入力版。
行単位という観点では、ほかにIO#readlineやIO#readlinesというのがあります。


#### IO#getc

入力から1文字だけ読み込んで返します。
とは言え、これもエンターキーを押すまでブロッキングされるcookedモードです。
エンターを押す前に2回以上キーボードが押されていた場合、2文字目以降はバッファに溜まったままになり、次回の入力操作時に残りが返されます。
IO#readcharも似たような動作をします。


#### IO#read(length)

基本的にはIO#readcharと同じ動作ですが、受け取りたい文字数を指定できます。cookedモードです。

### require "io/console" によりIOクラスへ追加されるメソッド

#### IO#getch

rawモードで1文字読み込んだ結果を返します。やりました遂にrawモードです。


#### IO#raw, IO#raw!, IO#cooked, IO#cooked!

"!"の付かないメソッドはブロック引数を取り、その中の端末モードをそれぞれraw、cookedに変更します。
"!"が付くメソッドは、それ以降のモードを恒久的に変更します。

----

つまり、素のRubyのIOは全面的にcookedモードです。
これはUnuxが基本的にcookedだから、そして、RubyがUnix大好きだからということだと思います（要出典）。


io/consoleをrequireすると、rawモードにまつわるメソッドがもろもろ追加されます。
これによってちょっと覚えにくくなるのが、 `IO#getc` と `IO#getch` ですね。
ともに1文字読み込みますが、前者はcookedモードで後者はrawモードです。


getch(3) はLinux標準ライブラリにはないけどWindowsにはある関数で、 `#include <conio.h>` すると使えるrawモードAPIです。
これに合わせたメソッドが、Rubyの `IO#getch` なんだろうと思います（要出典）。

## ブラウザ上のターミナルエミュレータ

話を戻すと、もともとのirb.wasmが利用していたターミナルのJSライブラリは[jQuery Terminal](https://terminal.jcubic.pl/)でした。
これはVT100ライクなターミナルエミュレータの仕様を大きく欠いています。


たとえばLinuxやMacやMinGWのターミナルで `ruby -e'print "\e[10C", "hello\n"'` と打つと、カーソルが10行右に移動した上で "hello" が表示されますね。
Relineはこのような制御を駆使して補完ダイアログを表示したりするのですが、jQuery Terminalはこういった制御ができません。


@mameさんや@kateinoigakukunさんによると、ターミナルエミュレータライブラリを[xterm.js](https://xtermjs.org/)に変更すればよさそうであることは、以前から認識されていました。
しかし、xterm.jsはrawモード端末なので、cookedモード端末を前提にしてつくられているRelineはそのままではうまく動作しないことも明らかでした。
それをなんとかした、というのが筆者による[今回のPR](https://github.com/kateinoigakukun/irb.wasm/pull/14)です。

![](/assets/images/202212/Reline___xterm.js_by_hasumikin___Pull_Request__14_.png)

## とはいえモンキーパッチです

実装の中心になっているのは[src/ruby/stdlib_compat.rb](https://github.com/kateinoigakukun/irb.wasm/pull/14/files#diff-156ccbf53cae75c2ae15b2833bba97ddf6689cc25744a956c0f4dfeee15e81e4)です。
Relineが利用しない `IO#gets` を削除して、 `IO#getch` や `IO#raw` などをモンキーパッチしてRelineをごまかしています。


当初@mameさんが可能性を指摘してくれたほどには、[PicoRubyシェル（R2P2）](https://github.com/picoruby/r2p2) の実装と今回のパッチに共通のコードはありませんが、R2P2をつくるときに学んだ端末制御についてのあれやこれやなノウハウはとても役に立ちました。さすが@mameさんですね。


それから、wasmについて@kateinoigakukunさんに質問しつつこの実装をしたので、ruby.wasmがどういうものであるかについておぼろげながら把握しました。
これを生かして、本業でもブラウザで動くRuby製業務ツールを絶賛開発中です。
ご本人の言葉を借りると、「@kateinoigakukunよりも先にruby.wasmのプロになったマン」です。

## Not only mruby but also CRuby

今回のこの活動と前後して、以前からRelineにパッチを送っていた@ima1zumiさんと同時に、ruby/relineのメンテナになりました。
これからはマイコンだけじゃなくCRubyもやっていき💪と決意を新たにした師走です。

----

Rubyアドベントカレンダーはあしたが最終日。
前田NaCl新社長が `"..."` について書くみたいです（また？）。
それでは良いお年を！

