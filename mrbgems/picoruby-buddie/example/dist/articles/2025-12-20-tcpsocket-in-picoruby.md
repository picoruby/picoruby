---
layout: default
title: PicoRubyのTCPソケットどうする？
date:   2025-12-20 09:00:00
categories:
---

これは[mrubyファミリ (組み込み向け軽量Ruby) Advent Calendar 2025](https://qiita.com/advent-calendar/2025/mruby-family)の12/20の記事です。

---

どうする？っていうか、もうつくっちゃっいました:

[https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-socket](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-socket)

## リソース優先の設計

以前から、picoruby-netというライブラリがありました:

[https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-net](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-net)

これはRubyKaigi 2024における@s01さんの発表 [*Adding Security to Microcontroller Ruby*](https://rubykaigi.org/2024/presentations/sylph01.html#day2) のなかで書かれました。

"Adding Security" というのは要するにTLS接続のことで、それを実現するにはそもそもHTTP通信できることが前提です。
だからpicoruby-netというものがつくられた、という訳です。

このpicoruby-netのAPIは以下のような感じ:

```ruby
require 'net'
client = Net::HTTPClient.new("example.com")
client.get("/")
=> {status: 200, headers: {...}, body: ...} みたいなやつ
```

つまり、CRubyのNet:HTTPとは互換性がありません。
CRubyのNet::HTTPは内部でTCPSocketクラスを使用します。
TCPSocketクラスは、このような継承によってつくられています:`TCPSocket < IPSocket < BasicSocket < IO < Object`

PicoRubyに、というかRaspberry Pi Pico WにHTTPクライアントをつくるのがどのくらいのメモリ消費になるのか、この時点では誰にもわかりませんでした。
ですから、PicoRubyのNet::HTTPClientはSocketとかIOとかそういう詳細をすっとばして直接的にLwIPを利用した実装にしたのは賢明な判断でした。

## picoruby-netで明らかになったこと

まず、Pico Wに載せたNet::HTTPSClient（HTTPじゃなくてHTTP**S**）でも、シンプルなPOSTメソッドが十分動くことがわかりました。
これはPicoRubyをIoTデバイスにするための必須要素です。
よしよし。

HTTPSのGETはメモリがきびしめです。
小さな結果しか返さないWebサーバとの通信なら、まあ動くかな、という感じです。
ただ、ここはもうすこし研究の余地があるかもしれません。
RSAよりECDSAの暗号スイートを優先するように処理すれば、OOMは減らせると思います（サーバが対応していれば。いまどきだいたい対応してるよね？）。

そうこうしているうちにPico 2 Wというメモリ2倍のボードが入手できるようになったので、「じゃあそろそろSocketつくっちゃおうか？」という機運が高まってきました。

## picoruby-socketの設計

PicoRubyのTCPSocketはこういう継承です: `TCPSocket < BasicSocket < Object`

UDPSocketは `UDPSocket < BasicSocket < Object`

CRubyよりも簡略化しました。まず、CRubyでもIPSocketを直接扱うことはあまりない（よね？）ので、これを省略。

IOクラスの継承は悩ましいけど、これも省略。
ベアメタルマイコンにおけるIOってやつの立ち位置がなかなか微妙で、これについてはそのうちちゃんと記事を書きます。書きたい。
っていうかこれがここ数年の最大のテーマ。

ともかく、`read`, `write`あたりのI/Oメソッドは個別のクラスにそれっぽく動くよう実装しました（ダックタイピングとでも言ったらいいでしょうか）。

## SSLSocketとSSLContextどげするか

CRubyではHTTPS通信に、`OpenSSL::SSL::SSLSocket`というものを使いますね。
言語設計のうえでは、OpenSSL以外のSSLContextを使用したTLSが可能ということになっています。
が、現実問題としてOpenSSL以外のTLS実装を利用することはないと思います（あげあげ）。

翻ってPicoRubyですが、MbedTLS以外を使用することはなさそうです。
というわけなので、`MbedTLS::SSL::SSLSocket`のようなややこしいものはつくらず、トップレベルにSSLSocketとSSLContextクラスをつくりました。
MbedTLSと密結合です。
べつにいいでしょ（そげか？）。

CRubyはこげ:

```ruby
require 'openssl'
require 'socket'

ctx = OpenSSL::SSL::SSLContext.new
soc = TCPSocket.new("example.com", 443)
ssl = OpenSSL::SSL::SSLSocket.new(soc, ctx)
```

PicoRubyはこげ:

```ruby
require 'mbedtls'
require 'socket'

ctx = SSLContext.new
soc = TCPSocket.new("example.com", 443)
ssl = SSLSocket.new(soc, ctx)
```

picoruby-socketをラップしたpicoruby-net-httpというのもつくってあります。
`require "net/http"`とすれば、CRubyなんとなく互換の`Net::HTTP`が使えます。

## 今後

今後はpicoruby-netは基本的にはあんまりメンテナンスしません。
ゆるやかに非推奨とします。
が、Pico Wではメモリ制約上picoruby-socketより有利だろうと思われますので、使いたい方はどうぞ。

さて、PicoRubyにTCPSocketがあるということは、ラズパイピコでdRubyが動くということですかね？（ばけばけ〜）
