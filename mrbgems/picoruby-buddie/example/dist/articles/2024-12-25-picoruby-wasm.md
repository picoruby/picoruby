---
layout: default
title: PicoRuby.wasmをつくった
date:   2024-12-25 08:00:00
categories:
---

この記事は、[mrubyファミリ Advent Calendar 2024](https://qiita.com/advent-calendar/2024/mruby-family)の25日目の記事であると同時に、[WebAssembly / Wasm Advent Calendar 2024](https://qiita.com/advent-calendar/2024/wasm)シリーズ2の25日目の記事です。
やったぜ最終日！

----

CRubyのWASM（ruby.wasm）はバイナリサイズが大きくて、ネットワークリソースが弱い環境では動作しない可能性が高まります。
一度ダウンロードできてしまえばあとはキャッシュがつかわれるはずだとは言うものの、リロードしたいケースは誰にでもあるでしょう。

軽量なオルタナティブがあり、利用者が目的にあわせて選択できるとしたら、それはそれでよいことです。
というわけで、試しにpicoruby.wasmをつくってみました。[^1]

[^1]: [https://www.npmjs.com/package/@picoruby/wasm-wasi](https://www.npmjs.com/package/@picoruby/wasm-wasi)

## 使い方

```html
<!DOCTYPE html>
<html>
  <head><meta charset="utf-8"></head>
  <body>
    <h1>DOM manipulation</h1>
    <button id="button">Click me!</button>
    <h2 id="container"></h2>
    <script type="text/ruby">
      require 'js'
      JS.document.getElementById('button').addEventListener('click') do |event|
        event.preventDefault
        JS.document.getElementById('container').innerText = 'Hello, PicoRuby!'
      end
    </script>
    <script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@0.9.6/dist/init.iife.js"></script>
  </body>
</html>
```

Rubyスクリプトをリモートから読み込むこともできます：

```html
    <script type="text/ruby" src="your_script.rb"></script>
```

### ※制限事項

現状では、`JS::Object#addEventListener` のブロックの中からブロックの外にあるローカル変数やインスタンス変数を参照できません：

```ruby
require 'js'
button = JS.document.getElementById('button')
button.addEventListener('click') do |event|
  event.preventDefault
  button.innerText = 'Clicked!'
  #=> buttonにアクセスできずNameErrorになる。なんとかしたいとは思うが....
  event.target.innerText = 'Clicked!' #=> これはOK
end
```

`addEventListener` 以外のブロックならこの制限はありません。

そのほか、言語仕様上の制限の多くはmruby/cに由来します。[^7]

[^7]: こちらを参照してください [https://qiita.com/HirohitoHigashi/items/14ffd29e1c23e6989191](https://qiita.com/HirohitoHigashi/items/14ffd29e1c23e6989191)

## 注目はバイナリサイズ

本日時点で公開しているpicoruby.wasm[^2]は、ダウンロードサイズ（brotli圧縮）が202KB、伸張後が610KBです。
対して、ruby.wasm[^3]はそれぞれ8.8MBと34MBです（筆者調べ）。

[^2]: https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@0.9.5/dist/picoruby.wasm

[^3]: https://cdn.jsdelivr.net/npm/@ruby/3.3-wasm-wasi@2.7.0/dist/ruby+stdlib.wasm

圧縮した状態で1/40よりさらに小さいので、「Rubyでブラウザをちょろっと動かしたい」くらいの用途にはpicoruby.wasmを選ぶインセンティブがあるかもしれません。

## 「async/awaitはあるのか？」「そんなものはない」

async/awaitを利用するためにはEmscriptenのビルドオプションに `-s ASYNCIFY=1` を追加する必要があります。
しかしAsyncifyを有効にすると、picoruby.wasmバイナリの圧縮前サイズが3倍以上の1.8MB超になってしまいました。
軽量軽薄をめざすpicoruby.wasmなので、Asyncifyを外し、async/awaitはサポートしないことにしました。
それに、Asyncifyはないほうが全体の動作が軽そうです。

いまのところ、`fetch` は以下のように書けます：

```ruby
require 'js'
# `then`は書かない。PromiseをRuby側に露出させない（いまのところ）
JS.global.fetch('example.jpg') do |response|
  if response.status.to_poro == 200
    response.to_binary #=> String（バイナリデータ）
  end
end
```

ブロック内でブロックします。
このほうがお気楽Rubyフロントエンド的にはわかりやすいんじゃないでしょうか。
`to_binary` も内部でarrayBufferのPromiseの完了を待ってブロックしています。
`then` は要らないと思いました。
`then` によるメソッドチェーンより、ネストしたRubyのブロックを書いたほうがきれいだから。

## 実行効率は？

これだとJSのイベントループを有効活用できないように思えますが、後述のとおりpicoruby.wasmの動作モデルがマルチタスクスケジューリングなので、ブロックされるのは「このタスクだけ」です。
fetch処理を別のタスクへ分割することによって、そのあいだも別の処理を動かせるんじゃないかと考えています（未検証）。

ところで、ruby.wasmにAsyncifyが使われているのはべつにasync/awaitをやりたいからじゃなく、CRuby実装にまつわるsetjmp/longjmpやスタックをスキャンする必要性（保守的GC）との関係らしいです。
Asyncifyを外したい気持ちはあるようです。[^4] [^5] [^6]

PicoRubyはsetjmp/longjmpを使っていないし、GCは魔のリファレンスカウンタだからAsyncifyが不要なのです。
やったね！

[^4]: ruby.wasmのasync/awaitについてはこの記事がわかりやすい [https://blog.tmtms.net/entry/202305-ruby-wasm-await](https://blog.tmtms.net/entry/202305-ruby-wasm-await)
[^5]: Asyncifyについては全Rubyist必読の『n月刊ラムダノート vol.4, no.1』（桃）をどうぞ [https://www.lambdanote.com/collections/n/products/nmonthly-vol-4-no-1-2024](https://www.lambdanote.com/collections/n/products/nmonthly-vol-4-no-1-2024)
[^6]: @kateinoigakukunによるRubyKaigi 2024のトーク [https://rubykaigi.org/2024/presentations/kateinoigakukun.html](https://rubykaigi.org/2024/presentations/kateinoigakukun.html)

## to_poro

上の `fetch` の例の中に `JS::Object#to_poro` というメソッドが使われています。
poroはPlain Old Ruby Objectの略です。
`to_i` とか `to_s` とかの変換パターンの全体像を考えるところまで手が回っていないので、とりあえず `to_poro` 内で条件分岐してそれっぽいRubyオブジェクトを返すという作戦です。
変換できないやつは `JS::Object` のままです。
以外と使い勝手がいいかもしれません。

## マルチタスク

CRuby1.9以降のThreadクラスはOSのネイティブスレッド（Linuxならpthread）のラッパです。
いまはWASM環境のスレッドサポートそのものが進化中らしく、ruby.wasmのThreadはまだ動いていないはずです（間違ってたら教えてください）。

他方、PicoRuby（のVMであるmruby/c）にはCRuby1.8のグリーンスレッドに似たタスクスケジューラがあります。
この仕組みをちょっとハックしてJSのメインループから制御することで、picoruby.wasmではマルチタスクが動くようになりました。
`sleep` もそれっぽく働きます（スリープ時間はあまり正確ではなく、研究中です）。

前述のとおり、`fetch` が重かったら別タスクに逃がすことができれば効率的に実行できるはずです。
でもどう書くのがよいのかはまだまじめに検討していません。
うまく書けたひとがいたら報告してください。
こう書ければいいけど微妙にできない！という報告や提案も大歓迎です。

以下は初歩的なマルチタスクのデモです：

```html
<!DOCTYPE html>
<html>
  <head>
    <meta charset="utf-8">
  </head>
  <body>
    <h1>Multi tasks</h1>
    <div>Open the console and see the output</div>
    <script type="text/ruby">
      while !(aho_task = Task.get('aho_task'))
        # Waiting for aho_task...
        sleep 0.1
      end
      i = 0
      while true
        i += 1
        if i % 3 == 0 || i.to_s.include?('3')
          aho_task.resume
        else
          puts "From main_task: #{i}"
        end
        sleep 1
      end
    </script>

    <script type="text/ruby">
      aho_task = Task.current
      aho_task.name = 'aho_task'
      aho_task.suspend
      while true
        puts "From aho_task: Aho!"
        aho_task.suspend
      end
    </script>

    <script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@0.9.5/dist/init.iife.js"></script>
  </body>
</html>
```

`<script type="text/ruby"> </script>` のかたまりを複数書くと、それぞれが別のタスクになって並行に稼働します。
これ以外にも動的にタスクを生成する方法はあるのですが、現状のAPIに満足できていないのできょうは省略。

## ブラウザでmrubyコンパイラが動いているのだ

そうなんですよ。
これはpicoruby.wasmだけに可能な応用になりますから、活用方法をひきつづき考えていきましょう。

## フロンエンドフレームワーク？

JSクラスだけでは実利用にはお堅いので、お気楽Rubyフロントエンドのフレームワークも開発中です。
使えそうなものができたら公開します。

## まだ機能不足

できたてほやほやなので、最低限未満の機能しか実装していません。
`fetch` はGETメソッドしか動きません。
エラー処理がほとんどない代わりに、バグはあるでしょう。
みなさまからの動作報告やパッチをお待ちしております。
年末年始はpicoruby.wasmで遊んでみてください。

そしてよいお年をお迎えください🎍

----
