# RBS TODO: steep check エラー分類

steep check の結果、**0件** のエラー/警告。全エラー解消済み！ 🫖

## 解決済み

- ~~`Ruby::NoMethod` (146件)~~ — PicoRuby固有のRBS定義の復活と型アノテーション追加で解消
- ~~`Ruby::UnannotatedEmptyCollection` (90件)~~ — 空配列・空ハッシュに `#: Type` 注釈を追加で解消
- ~~`Ruby::MethodParameterMismatch` (102件)~~ — `self.new` → `initialize` 変換、RBS修正で解消
- ~~`Ruby::MethodArityMismatch` (32件)~~ — Ruby側にオプション引数追加で解消
- ~~`Ruby::UndeclaredMethodDefinition` (26件)~~ — RBSにメソッド宣言を追加で解消
- ~~`Ruby::UnknownConstant` (6件)~~ — RBSに定数追加、Steepfileにignore追加で解消
- ~~`Ruby::AnnotationSyntaxError` (2件)~~ — 型注釈の構文修正で解消
- ~~`Ruby::ArgumentTypeMismatch` (66件)~~ — 型注釈、nilガード、RBS修正、steep:ignoreで解消
- ~~`Ruby::UnexpectedPositionalArgument` (43件)~~ — RBS修正、steep:ignoreで解消
- ~~`Ruby::MethodBodyTypeMismatch` (6件)~~ — steep:ignore（class<<self bug）、RBS修正、戻り値修正で解消
- ~~`Ruby::UnresolvedOverloading` (5件)~~ — RBS追加、型アノテーション、steep:ignoreで解消
- ~~`Ruby::ReturnTypeMismatch` (3件)~~ — RBS修正、steep:ignoreで解消
- ~~`Ruby::NoMethod` (3件)~~ — .to_s.to_symパターンで解消
- ~~`Ruby::BlockBodyTypeMismatch` (3件)~~ — steep:ignore（core RBSとの不一致）で解消
- ~~`Ruby::UnexpectedBlockGiven` (2件)~~ — steep:ignore（IO.new のRBS問題）で解消
- ~~`Ruby::MethodParameterMismatch` (1件)~~ — steep:ignore（gsub、core RBSとの重複回避）で解消
- ~~`Ruby::BlockTypeMismatch` (1件)~~ — steep:ignore（IO/Fileブロック型の不一致）で解消

## steep:ignore 使用箇所まとめ

以下は steep のバグまたは core RBS との互換性問題で steep:ignore を使用した箇所：

### steep バグ: `class << self` の `instance` 型解決

- `picoruby-dir/mrblib/dir.rb` — Dir.open
- `picoruby-vfs/mrblib/dir.rb` — Dir.open
- `picoruby-vfs/mrblib/file.rb` — File.open

### core RBS との互換性問題

- `picoruby-posix-io/mrblib/1_file.rb` — File.new の super（IO.new のRBS）
- `picoruby-posix-io/mrblib/1_file.rb` — File.foreach（Enumerator vs File）
- `picoruby-posix-io/mrblib/1_file.rb` — File.open ブロック戻り値型
- `picoruby-vfs/mrblib/file.rb` — File.open ブロック戻り値型
- `picoruby-posix-io/mrblib/3_kernel.rb` — Kernel#open（IO.popen, File.open）
- `picoruby-machine/mrblib/kernel.rb` — Kernel#p 単一引数の戻り値
- `picoruby-metaprog/mrblib/string.rb` — String#gsub パラメータ
- `picoruby-funicular/mrblib/component.rb` — normalize_vnode（VNode抽象型）
