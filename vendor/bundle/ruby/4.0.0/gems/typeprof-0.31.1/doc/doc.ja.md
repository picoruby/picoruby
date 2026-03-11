# TypeProf: 抽象解釈に基づくRubyの型解析器

## TypeProfの使い方 - CLIツールとして

app.rb を解析する。

```
$ typeprof app.rb
```

一部のメソッドの型を指定した sig/app.rbs とともに app.rb を解析する。

```
$ typeprof sig/app.rbs app.rb
```

典型的な使用法は次の通り。

```
$ typeprof sig/app.rbs app.rb -o sig/app.gen.rbs
```

## TypeProfの使い方 - Language Serverとして

[RubyKaigi 2024の発表資料](https://speakerdeck.com/mame/good-first-issues-of-typeprof)を参照ください。

## TypeProfの解析方法

TypeProfは、Rubyプログラムを型レベルで抽象的に実行するインタプリタです。
解析対象のプログラムを実行し、メソッドが受け取ったり返したりする型、インスタンス変数に代入される型を集めて出力します。
すべての値はオブジェクトそのものではなく、原則としてオブジェクトの所属するクラスに抽象化されます（次節で詳説）。

メソッドを呼び出す例を用いて説明します。

```
def foo(n)
  p n      #=> Integer
  n.to_s
end

p foo(42)  #=> String
```

TypeProfの解析結果は次の通り。

```
$ typeprof test.rb
# Revealed types
#  test.rb:2 #=> Integer
#  test.rb:6 #=> String

# Classes
class Object
  def foo : (Integer) -> String
end
```

`foo(42)`というメソッド呼び出しが実行されると、`Integer`オブジェクトの`42`ではなく、「`Integer`」という型（抽象値）が渡されます。
メソッド`foo`は`n.to_s`が実行します。
すると、組み込みメソッドの`Integer#to_s`が呼び出され、「String」という型が得られるので、メソッド`foo`はそれを返します。
これらの実行結果の観察を集めて、TypeProfは「メソッド`foo`は`Integer`を受け取り、`String`を返す」という情報をRBSの形式で出力します。
また、`p`の引数は`Revealed types`として出力されます。

インスタンス変数は、通常のRubyではオブジェクトごとに記憶される変数ですが、TypeProfではクラス単位に集約されます。

```
class Foo
  def initialize
    @a = 42
  end

  attr_accessor :a
end

Foo.new.a = "str"

p Foo.new.a #=> Integer | String
```

```
$ typeprof test.rb
# Revealed types
#  test.rb:11 #=> Integer | String

# Classes
class Foo
  attr_accessor a : Integer | String
  def initialize : -> Integer
end
```

## TypeProfの扱う抽象値

前述の通り、TypeProfはRubyの値を型のようなレベルに抽象化して扱います。
ただし、クラスオブジェクトなど、一部の値は抽象化しません。
紛らわしいので、TypeProfが使う抽象化された値のことを「抽象値」と呼びます。

TypeProfが扱う抽象値は次のとおりです。

* クラスのインスタンス
* クラスオブジェクト
* シンボル
* `untyped`
* 抽象値のユニオン
* コンテナクラスのインスタンス
* Procオブジェクト

クラスのインスタンスはもっとも普通の値です。
`Foo.new`というRubyコードが返す抽象値は、クラス`Foo`のインスタンスで、少し紛らわしいですがこれはRBS出力の中で`Foo`と表現されます。
`42`という整数リテラルは`Integer`のインスタンス、`"str"`という文字列リテラルは`String`のインスタンスになります。

クラスオブジェクトは、クラスそのものを表す値で、たとえば定数`Integer`や`String`に入っているオブジェクトです。
このオブジェクトは厳密にはクラス`Class`のインスタンスですが、`Class`に抽象化はされません。
抽象化してしまうと、定数の参照やクラスメソッドが使えなくなるためです。

シンボルは、`:foo`のようなSymbolリテラルが返す値です。
シンボルは、キーワード引数、JSONデータのキー、`Module#attr_reader`の引数など、具体的な値が必要になることが多いので、抽象化されません。
ただし、`String#to_sym`で生成されるSymbolや、式展開を含むSymbolリテラル（`:"foo_#{ x }"`など）はクラス`Symbol`のインスタンスとして扱われます。

`untyped`は、解析の限界や制限などによって追跡ができない場合に生成される抽象値です。
`untyped`に対する演算やメソッド呼び出しは無視され、評価結果は`untyped`となります。

抽象値のユニオンは、抽象値に複数の可能性があることを表現する値です。
人工的ですが、`rand < 0.5 ? 42 : "str"`の結果は`Integer | String`という抽象値になります。

コンテナクラスのインスタンスは、ArrayやHashのように他の抽象値を要素とするオブジェクトです。
いまのところ、ArrayとEnumeratorとHashのみ対応しています。
詳細は後述します。

Procオブジェクトは、ラムダ式（`-> { ... }`）やブロック仮引数（`&blk`）で作られるクロージャです。
これらは抽象化されず、コード片と結びついた具体的な値として扱われます。
これらに渡された引数や返された値によってRBS出力されます。

TODO: write more