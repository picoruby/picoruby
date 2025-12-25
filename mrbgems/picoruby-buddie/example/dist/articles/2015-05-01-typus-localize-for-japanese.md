---
layout: default
title: 管理アプリgem typusの日本語化【Rails4.2対応】
date: 2015-05-01 15:01:39.203804
categories:
---

Railsの管理画面用gemといえば [typus](https://github.com/typus/typus) ですね。
今回はこれを日本語化してみます。

### インストール

Gemfileに以下のように書くとバージョン4.2系をインストールできます。
ついでにページネーション用にkaminariも入れましょう。
rails-i18nは、Railsの多言語化を助けてくれるgemです。

```
# 追加するgems
gem 'typus', git: 'https://github.com/typus/typus.git', branch: '4-2-stable'
gem 'kaminari'
gem 'rails-i18n'
```

`bundle install`

`bundle exec rails g typus`

`bundle exec rails g typus:migration`

`bundle exec rake db:migrate`

以上のコマンドで、admin_usersテーブルがつくられます。

`bundle exec rails server`

として http://localhost:3000/admin/ に管理画面が出てくればここまではOK。

### 言語に日本語を追加

config/application.rb にこれを追記してRailsのデフォルトロケールを日本語に：

```
config.i18n.default_locale = :ja
```

config/initializers/typus_i18n.rb という名前でこのようなファイルを設置すると、typus内で日本語が有効になります：

```
module Typus
  module I18n
    class << self
      alias :old_available_locales :available_locales
      def available_locales
        self.old_available_locales.merge( "日本語" => "ja" )
        # 日本語だけつかえればいいなら、上の行を消して下の行を生かせばOK
        # {"日本語" => "ja"}
      end
      def default_locale
        :ja
      end
    end
  end
end
```

rails serverを再起動してください！

### 翻訳ファイル

しまもんのオレオレ翻訳ファイルです。各自でカスタマイズしましょう。

config/locales/typus.ja.models.yml：

```
# Japanese (ja) translations for Typus by:
# - shimamon

ja:
  activerecord:
    models:
      admin_user: "ユーザ"
    attributes:
      admin_user:
        email: "メールアドレス"
        first_name: "名"
        last_name: "姓"
        role: "権限"
        status: "有効 / 無効"
        locale: "言語"
        password: "パスワード"
        password_confirmation: "パスワード（確認）"
```

config/locales/typus.ja.yml：

```
# Japanese (ja) translations for Typus by:
# - shimamon

ja:
  "Actions": "アクション"
  "Active": "有効"
  "Admin": "ユーザ管理"
  "Add": "新規登録"
  "Add %{resource}": "%{resource} を新規登録"
  "Add New": "新規登録"
  "All": "すべて"
  "Application": "業務"
  "Are you sure?": "本当によろしいですか？"

  "Back to list": "一覧へ戻る"

  "Change %{attribute}?": "%{attribute} を変更しますか？"
  "Create %{resource}": "%{resource} を登録する"

  "Destroy": "削除"
  "Dashboard": "ダッシュボード"
  "Developed by": "Developed by"
  "Down": "下"

  "Edit": "編集"
  "Edit %{resource}": "%{resource} を編集"
  "Enter your email below to create the first user": "最初のユーザのメールアドレスを入力してください"
  "Error": "エラー"
  "Errors": "複数のエラー"

  "False": "False"
  "Filter": "検索"

  "I remember my password": "私はパスワードを覚えています"
  "If you didn't request a password update, you can ignore this message": "あなたがパスワード更新を要求しなかったのなら、このメッセージを無視してください"
  "If you have made any changes to the fields without clicking the Save/Update entry button, your changes will be lost": "保存 / 更新ボタンを押さない場合、あなたが入力した変更は失われる可能性があります"
  "Inactive": "無効"

  "Last few days": "最近の数日"
  "Last 30 days": "最近30日間"
  "Last 7 days": "最近7日間"
  "Loading": "読み込み中"
  "Login": "ログイン"

  "New": "新"
  "New %{resource}": "新しい %{resource}"
  "Next": "次"
  "No %{resources} found": "%{resources} は見つかりませんでした"
  "No entries found": "データがありません"

  "Or": "または"

  "Password recovery link sent to your email": "パスワードリカバリのためのメールがあなたのメールアドレスへ送信されました"
  "Please set a new password.": "新しいパスワードを入力してください"
  "Previous": "前"

  "Record moved %{to}": "Record spostato %{to}"
  "Recover password": "Recupera la password"
  "Remove": "削除"
  "Remove %{resource}?": "%{resource} を削除しますか？"
  "Reset password": "パスワードをリセット"
  "Resources": "データ"

  "Site administration": "管理メニュー"

  "Save %{resource}": "%{resource} を保存する"
  "Save": "保存"
  "Save and add another": "保存してさらに新規登録する"
  "Save and continue editing": "保存して編集を続ける"
  "Search": "検索する"
  "Show": "見る"
  "Show all dates": "すべての日付を見る"
  "Show by %{attribute}": "%{attribute} でフィルタリング"
  "Show %{resource}": "%{resource} を見る"
  "Sign in": "サインイン"
  "Sign out": "サインアウト"
  "Sign up": "サインアップ"
  "Submit": "決定"
  "System Users Administration": "システムユーザ管理"

  "Today": "きょう"
  "Trash": "削除しますか"
  "True": "True"

  "Up": "上"

  "View all %{attribute}": "すべての %{attribute} を見る"
  "View site": "サイトを見る"

  "You can update your password at": "ここでパスワードを更新できます"
  "You can't change your role": "あなたは権限を変更できません"
  "You can't toggle your status": "あなたはアカウントの有効 / 無効を変更できません"
  "Your new password is %{password}": "新しいパスワードは %{password} です"

  "%{model} successfully created": "%{model} をつくりました"
  "%{model} successfully removed": "%{model} を削除しました"
  "%{model} successfully updated": "%{model} を更新しました"
```

### 「ユーザs」みたいになるのを避ける

typusのview内では、モデル名に対して#pluralizeメソッドが呼ばれています。
そうすると、

```
ja:
  activerecord:
    models:
      admin_user: "ユーザ"
```

のように日本語化したモデルがムリヤリ複数形化されて `ユーザs` と表示されてしまいます。
そこで config/initializers/inflections.rb を以下のようにすると、アルファベットで終わらないモデル名にはsがつかなくなります：

```
# pluralizeメソッドはつねに:enがデフォルト引数。だからこうすることで「日本語s」を避けられる
ActiveSupport::Inflector.inflections(:en) do |inflect|
  inflect.plural(/^.*[^A-Za-z]$/, '\0')
  inflect.singular(/^.*[^A-Za-z]$/, '\0')
end
```

これで日本語化された管理画面が手に入りました！


#### しまもんとお約束だよ！

これらのコードをつかって発生するいかなる不具合、損害にも責任は負わないよ！
自己責任でよろしくね！


