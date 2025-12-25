---
layout: default
title: RBSとSteepとPicoRubyのちょっといい話
date: 2021-05-24 08:54:58.434666
categories:
---

弊グループのエンジニアリングブログに記事を書きました。


[Static type checking in PicoRuby / Monstarlab's Engineering Blog](https://engineering.monstar-lab.com/2021/05/27/Static-type-checking-in-PicoRuby)


ここには結論だけ書いておきますので、気になる方は本文をお読みください。

### 結論ッ!!

- RBSは、Rubyプログラムの型に関する情報を記述するための独立した言語です
- Steepは、RBS形式で書かれた情報を元に、Rubyプログラムを静的に型チェックします
- CRubyの文法自体が、静的型チェックをサポートしているわけではありません
- それ故、他のRuby言語実装もRBSとSteepを**そのまま**利用できます。そう、**PicoRuby**もね！
- 静的型チェックの便利さは、ファームウェアプログラミングで際立つのであります。デバイスに書き込む前につまらないミスを見つけたいから！

![helix](/assets/images/202105/helix_rev3.jpg)
