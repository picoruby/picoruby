# RBS TODO: steep check エラー分類

steep check の結果、残り **32件** のエラー/警告。

## 残存エラー一覧

| Diagnostic ID | 件数 | 説明 |
|---|---|---|
| `Ruby::MethodBodyTypeMismatch` | 6 | メソッド本体の型が宣言と合わない |
| `Ruby::UnresolvedOverloading` | 5 | オーバーロードの解決失敗 |
| `Ruby::UnexpectedPositionalArgument` | 4 | RBS宣言より多い位置引数 |
| `Ruby::ArgumentTypeMismatch` | 4 | 引数の型が宣言と不一致 |
| `Ruby::ReturnTypeMismatch` | 3 | 戻り値の型が不一致 |
| `Ruby::NoMethod` | 3 | 型にメソッドが存在しない |
| `Ruby::BlockBodyTypeMismatch` | 3 | ブロック本体の型不一致 |
| `Ruby::UnexpectedBlockGiven` | 2 | 予期しないブロック渡し |
| `Ruby::MethodParameterMismatch` | 1 | メソッドの引数がRBS宣言と不一致 |
| `Ruby::BlockTypeMismatch` | 1 | ブロック引数の型不一致 |

## 残存エラー詳細

### Dir の self 型問題（カテゴリE、未着手）

- `picoruby-dir/mrblib/dir.rb:41` — `Dir` を `singleton(Dir)` として渡せない
- `picoruby-vfs/mrblib/dir.rb:7` — `Dir` を `instance` として渡せない
- `picoruby-vfs/mrblib/dir.rb:29` — `Dir` を `String` として渡せない

### File.new / super 呼び出し（IO.new のRBS問題）

- `picoruby-posix-io/mrblib/1_file.rb:9,12` — `super` に位置引数・ブロックを渡せない（IO.new のRBS定義が合わない）

### File.foreach の戻り値型

- `picoruby-posix-io/mrblib/1_file.rb:182,186` — ブロックなし時に `File` を返すが、core RBS は `Enumerator | nil` を期待

### File.open / load_file のブロック戻り値

- `picoruby-posix-io/mrblib/1_file.rb:20` — `File.open` ブロックの戻り値型が `bool` と宣言されている
- `picoruby-vfs/mrblib/file.rb:99` — 同上

### String#gsub パラメータ

- `picoruby-metaprog/mrblib/string.rb:14` — gsub のパラメータがcore RBSと不一致

### VFS File#each_line 戻り値

- `picoruby-vfs/mrblib/file.rb:140` — `each_line` のメソッド本体が `nil` だが `self | Enumerator` を期待

### Kernel#open ブロック型

- `picoruby-posix-io/mrblib/3_kernel.rb:12` — `IO` ブロックを `File` ブロックとして渡す（steep:ignore 済み）

### Net の戻り値型

- `picoruby-net/mrblib/net.rb:29` — 戻り値の Hash 型が `httpreturn` 型と合わない

### Rapicco parser の NoMethod

- `picoruby-rapicco/mrblib/parser.rb:96,98,100` — `String?` に対する `to_sym` 呼び出し

### Funicular child_t 型

- `picoruby-funicular/mrblib/component.rb:352` — `child_t` に `String` を追加したことで `VNode` → `child_t` の変換が必要に

## 解決済み

- ~~`Ruby::NoMethod` (146件)~~ — PicoRuby固有のRBS定義の復活と型アノテーション追加で解消
- ~~`Ruby::UnannotatedEmptyCollection` (90件)~~ — 空配列・空ハッシュに `#: Type` 注釈を追加で解消
- ~~`Ruby::MethodParameterMismatch` (102件)~~ — `self.new` → `initialize` 変換、RBS修正で解消
- ~~`Ruby::MethodArityMismatch` (32件)~~ — Ruby側にオプション引数追加で解消
- ~~`Ruby::UndeclaredMethodDefinition` (26件)~~ — RBSにメソッド宣言を追加で解消
- ~~`Ruby::UnknownConstant` (6件)~~ — RBSに定数追加、Steepfileにignore追加で解消
- ~~`Ruby::AnnotationSyntaxError` (2件)~~ — 型注釈の構文修正で解消
- ~~`Ruby::ArgumentTypeMismatch` (58件→4件)~~ — 型注釈、nilガード、RBS修正で大幅削減
- ~~`Ruby::UnexpectedPositionalArgument` (43件→4件)~~ — RBS修正で大幅削減
