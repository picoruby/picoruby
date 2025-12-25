---
layout: default
title: RubyKaigi2019で発表した内容について〈mrubyc-debugger編〉
date: 2019-04-23 13:57:24.671181
categories:
---

[前回の記事はこちら](http://shimane.monstar-lab.com/hasumin/RubyKaigi2019-mrubyc-test)

## mrubyc-debugger

さて続きましてmrubyc-debuggerです。


[https://github.com/hasumikin/mrubyc-debugger](https://github.com/hasumikin/mrubyc-debugger)


こちらもgemです。CRubyです。去年の12月くらいからちょこちょこつくりはじめたツールで、いまでもexperimentalです。でも実際はそこそこ実用になる気がしています。

![](https://raw.githubusercontent.com/wiki/hasumikin/mrubyc-debugger/images/demo-2.gif)

マイコンのファームウェアではステートマシンを内部にもち、動作の選択肢をそこに紐付けるような設計が多いと思います。
そんなとき、ループをぐるぐる回してステートのあらゆる可能性を視覚的に追いかけるというのは悪くない発想だし、さらに機能追加してステートマシン表みたいのを自動生成するような仕組みもあるかなーと考えているところです。これも来年のRubyKaigiネタになるだろうか。。。？

### TracePoint友の会

登壇のときには話さなかった技術上のネタをひとつ。ブレークポイントから起動するデバッグウィンドウのコードの一部です。

```
            begin
              result = if args.is_a?(Array)
                tp_binding.local_variable_set(args[0], eval(args[1]))
              else
                if tp_binding.local_variables.map(&:to_s).include?(args)
                  tp_binding.local_variable_get(args)
                else
                  eval(args)
                end
              end
              win << result.to_s[0, win.maxx - 7]
            rescue => e
              win << e.to_s[0, win.maxx - 7]
            end
```

デバッグ対象のループスクリプトのローカル変数を上書きする機能です。TracePointがくれたbindingオブジェクトに、ユーザが入力した `変数 = 値` をわざわざ分解して

```
tp_binding.local_variable_set(args[0], eval(args[1]))
```

このようにセットしています。bindingオブジェクトは#evalが呼べるのだから、

```
tp_binding.eval(arg)
```

こうでも良さそうです。が、動きません。rescueさえされず、音もなく（？）Rubyが落ちます。
TracePointがフックした側のスレッドではちゃんと#evalできるんです。しかし、デバッグコンソール側のスレッドで#evalすると無言で（？）落ちます。


（ただし検証が足らない可能性はかなりあります。とくに、本当に無言で落ちているかどうかは自信ないです。straceで追いかけることさえやれていません。。。時間があったらもっと検証したいけど、これからしばらく忙しくなるので、ひとまず現状の認識のまま書いてしまいます）


このことを@shugoさんに話したら、「TracePointで拾ったものを別のスレッドに渡すなんてダメですよ（フッ）」ってたしなめられてしまいましたｗ


要するにツールとしての基本アイデアがダメダメであるっていうわけなのですが、まつもとさんからは「あれすごいねー」と言っていただいたので、動くあいだは開発を続けてみます。


mrubyc-testがmrubyにも使えないわけではなさそうなのと同様に、こちらもmruby/c限定にしなくても、なんならふつうのCRubyのマルチスレッド系ツールのデバッガに使えるかもしれませんが、そんなこんななのでやめたほうがいいでしょう。

TracePointつかってバグったときの壊れ方というかスタックトレースの量が衝撃的だし。


mruby/c専用ということにして問題のスコープを小さく保つのがよさそうです。
