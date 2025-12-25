---
layout: default
title: Reline::Faceで快適ターミナル生活
date: 2023-11-28 14:32:42.370512
categories:
---

これは[Ruby Advent Calendar 2023](https://qiita.com/advent-calendar/2023/ruby)の12月1日の記事です。
つまり一発目です。
お手柔らかにお願いします。

-----

わたくしは1年くらい前に、[ruby/reline](https://github.com/ruby/reline)のメンテナになりました。
その数ヶ月後には[ruby/irb](https://github.com/ruby/irb)のメンテナにもなりました。
Relineのメンテナになった当初のミッションを1年かけて解決した（と思う）、でもまだやることはあります、という記事を書きます。

## 何を解決したか――色が薄くて見えにくい問題

結論から書きます。
IRBの補完ダイアログは、薄い青地に白い文字だったため、視認性があまりよくありませんでした。
下のキャプチャは筆者の環境のもので、視認性はそんなに悪くありません。

![](/assets/images/202312/reline-before-face.png)

しかしターミナルエミュレータやディスプレイごとに実際の色（の見え方）が異なります。
一部のユーザにとっては我慢できるものではなく、色づけ機能をオフにしていた人も多くいたと思われます。


今回わたくしがコミットした `Reline::Face` によって、補完ダイアログの色をみなさんが設定できるようになりました。
おめでとうございます。ありがとうございます。

## Relineとは――Ruby版のReadline

Relineとは大ざっぱにいうと、Ruby版のReadlineです。
Readlineとは、UNIX系システムで使用される、コマンドラインインタフェース（CLI）でのテキスト入力のためのライブラリです。
コマンドライン上でのテキストの読み込みや編集、入力履歴などを管理します。

ReadlineをRelineに置き換えるメリットは、おそらく2つあります。
ひとつは、C拡張をなくせること。
libreadline-devがないせいでRubyのインストールが失敗した経験、だれにでもありますよね？
Readlineにはクロスプラットフォームや多言語対応の問題もあり、あらゆる環境で同等に動かすためのテストも容易ではありません。
他方、RelineはピュアRubyで書かれているのでプラットフォーム依存がなくなります（少なくとも、発生した問題をRubyコミュニティだけで解決できます）。
そしてRubyのインストールが簡単になったのは、入門者を増やしたいRubyコミュニティにとっても重要な改善です。


もうひとつは、IRBの機能向上です。
もうみなさん当たり前のものとして受け入れているかと思いますが、複数行の履歴編集やメソッドの補完ダイアログやドキュメントの表示機能は、Relineのお陰で実現したものです。
RelineはRuby2.7にバンドルされました。
その後、`irb --legacy` とすればReadline版のIRBを使用できるようになっていましたが、Ruby3.3はついにReadlineのサポートを削除した状態で出荷されます。

## Reline::Faceの使い方――とりあえず.irbrcに書く

Ruby3.3のリリース前でも、 `gem install reline` すれば使えます。
`~/.irbrc` に下のコードをコピペしてからIRBを起動してください。

```ruby
Reline::Face.config(:completion_dialog) do |conf|
  conf.define :default, foreground: :white, background: :blue
  conf.define :enhanced, foreground: :white, background: :magenta
  conf.define :scrollbar, foreground: :white, background: :blue
end
```

どうでしょうか、補完ダイアログの色が下のキャプチャのようになったと思います。
詳しい使い方は[ドキュメント（ruby/ruby/doc/reline/face.md）](https://github.com/ruby/ruby/blob/master/doc/reline/face.md)を参照してください。

![](/assets/images/202312/reline-after-face.png)

それから、すでに色づけをgem化しているアニキもいます：[https://github.com/katsyoshi/irb-theme-dracula](https://github.com/katsyoshi/irb-theme-dracula)

## Windows terminalの人はもうひとつやることがあります

.irbrcに下記を追記してください。

```ruby
Reline::Face.force_truecolor
```

これはなにかと言うと、ターミナルエミュレータにTruecolorの使用を強制する、というものです。
多くのターミナルエミュレータでは、環境変数 `$COLORTERM` に `truecolor` という値を保持することで、自信がTruecolor対応端末であることをユーザプログラムに伝えます。
しかし、Windows terminalはTruecolorに対応した実装であるにもかかわらず、環境変数 `$COLORTERM` の値がセットされていません。
Relineは `$COLORTERM` の値に"truecolor"という文字列が含まれない場合、256色端末モードにフォールバックします。
`foreground: "#3d0f22"` のようにTruecolorを指定されても、256色から選べる近い色へと情報を落としてしまいます。
これを防ぐため、Windows terminal遣いの人は `Reline::Face.force_truecolor` を書く必要がある、というわけです。


## Future work

まだすべてが解決してはいません。
IRBは補完ダイアログ内で選択されたメソッドのドキュメンテーションを右側に表示します。
riコマンドのさらなるインタラクティブ版？です。
これの色の変更には未対応ですので、やっていきたいなと思います。

-----

以上、皆様のターミナル生活が色鮮やか、こころ健やかなものになりますよう、お祈り申し上げる次第です。


あしたのアドベントカレンダーは、@katsyoshiさんがなにかを書くみたいです！
