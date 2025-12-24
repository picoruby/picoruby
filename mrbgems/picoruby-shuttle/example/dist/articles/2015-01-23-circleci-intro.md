---
layout: default
title: CircleCIで継続的インテグレーション【導入編】
date: 2015-01-23 11:39:03.992792
categories:
---

Travis CIはお値段が高いよねってことで[CircleCI](https://circleci.com)を試してみます。

われわれのアプリはもちろんRuby on Railsです。

### まずは登録

![登録](/assets/images/201501/hasumin_cirleci_intro_1.png)

弊社のGithubチームが管理しているリポジトリの一覧がでました。どうやら管理者に承認もらわないといけないようです。Contact repo adminを押して、別ルートでも社内の管理者に承認をお願いしました。
無事登録完了。

### これだけでCIがはじまります

なにも設定とかしてませんが、Githubにプッシュするだけでビルド〜テストが実行されます。まあでもいきなりオールグリーンとか期待してませんよええ。

![最初の失敗](/assets/images/201501/hasumin_cirleci_intro_3.png)

bundle installで失敗しているようです。どうやら[gem byebug](https://github.com/deivid-rodriguez/byebug)がインストールできない模様。われわれはRuby2.2.0をつかっているのですが、CircleCIのデフォルト環境では1.9.xみたいです。

### circle.yml

ここではじめてCircleCI設定ファイルを書くことになりました。プロジェクトディレクトリの直下に circle.yml を設置します。

```
machine:
  timezone:
    Asia/Tokyo
  ruby:
    version: 2.2.0
```

内容はこれだけ。これにより `bundle install` はクリアしました。ですが……

### 謎の失敗

![謎の失敗](/assets/images/201501/hasumin_cirleci_intro_2.png)

`bundle exec rake db:create db:schema:load --trace` のところでFailします。

![手動ビルドボタン](/assets/images/201501/hasumin_cirleci_intro_7.png)

ビルド後のVMにsshできるようにするという手動ボタンがあるので、これを押し、当然おなじ場所でFailした後にsshしてみます。
すると、テーブルはつくれているし `rails console` もちゃんと起動します。

謎。

いったん諦めました……。

### と思ったら、翌日

![Timからメール](/assets/images/201501/hasumin_cirleci_intro_4.png)

Circleの中の人からメールがきました。いえ、こちらからは問い合わせてないんです。どうやらFailedが連続しているのを検知して、能動的サポートをしてくれたようです。ワンダホー！

というわけで、そうなんだよオレもどうしたもんかわかんないんだよ、って返事したところ、さっそく別の担当者が返信くれました。

![Robertからメール](/assets/images/201501/hasumin_cirleci_intro_5.png)

曰く、

> db:load のなかでminitestが動いちゃってるけどマズイよね、変なとこでrequireしてない？

！！！！
Gemfileをみたら……

```
group :test do
  gem 'minitest'
  gem 'minitest-rails'
  gem 'minitest-doc_reporter'
  gem 'minitest-stub_any_instance'
  gem 'minitest-bang'
  gem 'minitest-line'
  gem 'factory_girl_rails'
  gem 'json_expressions'
end
```

Oh...
そんなわけで

```
group :test do
  gem 'minitest',                   require: false
  gem 'minitest-rails',             require: false
  gem 'minitest-doc_reporter',      require: false
  gem 'minitest-stub_any_instance', require: false
  gem 'minitest-bang',              require: false
  gem 'minitest-line',              require: false
  gem 'factory_girl_rails',         require: false
  gem 'json_expressions',           require: false
end
```

こうしたら、

![Robertからメール](/assets/images/201501/hasumin_cirleci_intro_6.png)

／(^o^)＼

本当はこの間、`npm test` だの `gulp test` だのの問題もあったのですが、それはまた別の機会に書きます。
