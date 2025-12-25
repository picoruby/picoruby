---
layout: default
title: RequireJS環境のBackbone.Modelインスタンスを複数のViewから共有する
date: 2015-01-30 13:46:25.707406
categories:
---

「[Backbone.jsでOAuth2.0のためのAuthorizationヘッダを仕込む](/hasumin/backbone.js-oauth2.0-authorization-header)」では、アクセストークンをクッキーに保存していました。

ではたとえば、クッキーに入れるな、ブラウザ閉じたらアクセストークン消えても構わない、っていう要件だったらどうしますか？

### Backbone.Modelに入れてみましょうか

普通、Modelモジュールってこう書きますよね：

ファイル名：*AccessTokenModel.coffee*

``` coffeescript
define [
  'backbone'
], (Backbone) ->
  AccessTokenModel = BackboneModel.extend({})
  return AccessTokenModel
```

でこいつを某Viewからつかうわけです、

``` coffeescript
define [
  'backbone', 'AccessTokenModel'
], (Backbone, AccessTokenModel) ->
#（略）
  AccessToken = new AccessTokenModel()
#（略）
```

でもRequireJS使っていると、別のViewから件のModelインスタンスを取りに行くのメンドウですよね。まさかとは思いますが、

``` coffeescript
window.AccessToken
```

なんて書いたら負けですよね。せっかくのCoffeeScriptなのに。

### じゃあこうしましょう

Modelモジュールの書きかたをこのように変更します。

ファイル名：*TheAccessToken.coffee*

``` coffeescript
define [
  'backbone'
], (Backbone) ->
  AccessTokenModel = BackboneModel.extend({})
  TheAccessToken = new AccessTokenModel()
  return TheAccessToken
```

ワタシJavaScript用語ワカラナーイのでRubyぽく表現しますが、クラスを返すんじゃなくて、インスタンスを返せばいいんです。

するってぇと某Viewはこう書けます。あとはRequireJSがうまいことやってくれます：

``` coffeescript
define [
  'backbone', 'TheAccessToken'
], (Backbone, TheAccessToken) ->
# （略）
  login: (params) ->
    # ログイン処理の例
    # エラー判定などは省略しています
    TheAccessToken.save(params)
    Backbone.history.navigate('/authed_area', true)
  logout: ->
    #ログアウト処理の例
    TheAccessToken.destroy()
    Backbone.history.navigate('/', true)
```

そうです。new が不要です。そして、TheAccessTokenをrequireしているすべてのモジュールで、**同じアクセストークンインスタンスを共有**できます。

### 応用

たとえばこれがアクセストークンじゃなくてユーザデータのモデルだったとしましょうか。

画面のヘッダViewに `TheUser.get('name')` 、ナビViewに `TheUser.get('status')` を表示したいっていう場合でも、各Viewが「TheUser」をrequireするだけでOKです。

オブジェクト名にTheをつけて、通常用法のModelモジュールと区別するのがしまもんのマイブームです。


#### しまもんとお約束だよ！

うまく動かなくても責任もちませんのであしからずご了承ください。
