---
layout: default
title: CRubyとmrubyのメソッド可視性（privateとpublic）について
date:   2024-12-17 08:00:00
categories:
---

この記事は、[Ruby Advent Calendar 2024](https://qiita.com/advent-calendar/2024/ruby)の17日目の記事であると同時に、[mrubyファミリ Advent Calendar 2024](https://qiita.com/advent-calendar/2024/mruby-family)の17日目の記事でもあります。

----

みなさんこんにちは。
ほんじつはprivateとpublicについて書きます。[^1]

mrubyはCRubyとほぼ同じ言語機能を実装していますが、メソッドの可視性に関する機能はなぜか実装されていません。

[^1]: 本稿では、[世界のshugomaeda](https://twitter.com/shugomaeda)が実装したと伝わる`protected`については触れません。だれも使ってないので。

## メソッドの可視性とは（復習）

`private`より後ろに定義したメソッドは、クラスの外から呼べません：

```ruby
# CRuby
class MyClass
  private

  def private_method
    "Private Method"
  end
end

MyClass.new.private_method
#=> private method 'private_method' called for an instance of MyClass (NoMethodError)
```

自クラスの中からなら呼べます：

```ruby
# CRuby
class MyClass
  def call_private_method
    private_method
  end

  private

  def private_method
    "Private Method"
  end
end

MyClass.new.call_private_method
#=> Private Method
```

言い方を換えると、privateなメソッドは関数形式でしか呼べません。
関数形式とは「レシーバを書かず、いきなりメソッドを書く」形式のことです。[^2]

[^2]: `self`がレシーバになるときは、privateなメソッドも呼べます。

## mrubyではどうなのか？　そしてputs

ところがmrubyでは、`private`と書いてもとくに効果が発揮されず（なぜかエラーにもならず）、`MyClass.new.private_method`が動作します。
ということはつまりですね、こういうコードも動いてしまいます：

```ruby
# mruby
[].puts "Hello" #=> Hello
```

Arrayインスタンスに`puts`メソッドが実装されているかのように動いてしまいましたね。
CRubyでは、これは動きません。
でも実はCRubyでも、Arrayインスタンスには`puts`メソッドが継承されているのです。
なぜならKernelモジュールにputsインスタンスが書かれていて、以下の順序でそれが継承されています：

```
Array < Object < Kernel
```

CRubyで`[].puts "Hello"`が動作しないのは、`Kernel#puts`がprivateなメソッドだからです。
privateなメソッドは関数形式でしか呼べないので、レシーバを明示した書き方はダメなのです。

こう書けば動きます：

```ruby
# CRuby
[].send(:puts, "Hello") #=> Hello
```

後述する`Module#public`の用法により、これもいけます：

```ruby
# CRuby
class Array
  public :puts
end

[].puts "Hello" #=> Hello
```

## トップレベルに定義したメソッドの可視性

そしてこれは、意外と知られていないかもしれない、あるいは広く一般には意識されていないであろうCRubyの言語仕様です：

```ruby
# CRuby
def top_level_method
  "You cannot see this"
end

# `class MyClass < Object; end` ←これと同じ
class MyClass
end

MyClass.new.top_level_method
#=> private method 'top_level_method' called for an instance of MyClass (NoMethodError)
```

NoMethodErrorになるのはなぜでしょう？
トップレベルに定義した`top_level_method`はObjectクラスのインスタンスメソッドになります。
クラス定義時にSuperクラスを明示しない場合、Objectクラスを暗黙的に継承することは皆さん知っていると思います。
それならば`MyClass#top_level_method`は呼べるはずですが、呼べません。

なぜなら、エラーメッセージから想像できるとおり、CRubyというRuby実装では、トップレベルのメソッドを暗黙的にprivateに定義するからです。
ですから、以下のように書けば動きます：

```ruby
# CRuby
public def top_level_method # publicを明示的に書く
  "You can see this!"
end

class MyClass
end

MyClass.new.top_level_method
#=> You can see this!
```

トップレベルにメソッドを定義するのはたいてい書き捨てスクリプト（？）でしょうから、この仕様が適切なのはよくわかりますね。
あらゆるクラスに`top_level_method`を生やしたいわけじゃないので。

ただ、以下のコードは動くのです：

```ruby
# CRuby
def top_level_method
  "You cannot see this、と思うじゃん？"
end

class MyClass
  def my_method
    top_level_method
  end
end

MyClass.new.my_method
#=> You cannot see this、と思うじゃん？
```

`top_level_method`を関数呼び出ししているから呼べる。
それはそう、なのですが、これは呼べなくていいやつという気がしませんか？

## mrubyでは

これまでの説明でわかったと思いますが、mrubyならば、Objectクラスを継承しているあらゆるインスタンスからトップレベルメソッドが呼べます：

```ruby
# mruby
def top_level_method
  "You cannot see this（見えとるがな）"
end

class MyClass
end

MyClass.new.top_level_method
#=> You cannot see this（見えとるがな）
```

筆者の疑問は、なぜmrubyでは`private`や`public`を書いてもエラーにならず、しかし効果をなにも発揮しないのか？という点です。

エラーを出さないという選択についてひとつ言えそうなのは、`private`を使用しているCRuby用のスクリプトをそのままmrubyで動かすときにエラーになると面倒くさいから（べつにちゃんと動くしね）、ということかなと思います。

しかしそもそもなんで可視性機能を実装しないのでしょうね？
まつもとさんに質問しようと以前から思っているのですが、なぜかご本人に会うと忘れていて聞きそびれてしまうので、ここに書いておきます。
だれが聞いてください。

## もうひとつの都合？

こんな理由もあるかもしれません。
`private`や`public`の実体は`Module#private`と`Module#public`です。
これらのメソッドは2種類の振る舞いを持っています。

以下のように書くとき、`private`メソッドは引数`:private_method`を受け取ります：

```ruby
# CRuby
private def private_method
  "private_method"
end

def public_method
  "public_method"
end
```

なぜなら、def式はメソッド名のシンボルを返すからです。
`private(:private_method)`が実行されることにより、`private_method`メソッドがprivateになります。
だから、後続の`public_method`はprivateなメソッドになりません。
これがひとつ目の振る舞い。

しかし、以下のように書くと、動作が異なります：

```ruby
# CRuby
private

def private_method
  "Private Method"
end

def public_method # これもprivateになる
  "Public Method（嘘です）"
end
```

こちらの書き方のほうが広く使われているかもしれません。
引数をとらない`private`は、それ以降のメソッド定義をprivateにします。
言い換えると、コンテキストを変更するメソッドなのです。
これはmrubyとの相性が悪いかもしれない。

mrubyコンパイラ（`mrbc`コマンド）は、コマンドライン引数に複数のRubyスクリプトファイルパスを受け取ることができ、それらのファイルの中身を連結（ほんとうに単なる連結）してひとつのVMコードへとコンパイルします。
すべてが連結された状態でRubyとしてVaildであればよいので、クラス定義の途中で分割したファイルを気ままに連結しても構いません。
このとき、意図せずにprivateコンテキストになっているせいでバグるかもしれず、これにまつわる問題を華麗に解決できないから放置しているのかもしれません。
知らんけど。

とはいえ、mruby界隈でそんなアクロバティックなスクリプト分割をしている人は見たことがありません。
この理由はとくに重要ではなさそうです。
蛇足でした。

## putsは偉い

`puts`メソッドは、世界のshugomaedaがその名前を提案してRuby言語に取り込まれた機能だそうです。
`print`や`println`より文字数が少ないから偉いんだそうです。
ところで、みなさんは`puts`をどう発音しますか？

筆者は「プットエス」と発音します。
太古の昔から「プッツ」と発音する人が多いことは知っていますが、`putc`を「プッチ」と発音する人には会ったことがありません。
だから「プッツ」には違和感があります。
「プッツ」と「プットシー」では一貫性がないじゃないですか？
長々とした記事になりましたが、これを言いたいためにこの記事を書きました。

それではよいお年をお迎えください！

----
