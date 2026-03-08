# RBS TODO: steep check エラー分類

steep check の結果、全 **297件** のエラー/警告を分類した。

## 1. RBS定義の不足・不整合系（134件）

RBS側にメソッドや定数の定義を追加・修正すれば解消する。

| Diagnostic ID | 件数 | 説明 |
|---|---|---|
| `Ruby::MethodParameterMismatch` | 102 | メソッドの引数がRBS宣言と不一致（`initialize`のパラメータ未宣言が多い） |
| `Ruby::UndeclaredMethodDefinition` | 26 | RBSに宣言されていないメソッド定義 |
| `Ruby::UnknownConstant` | 6 | RBSに宣言されていない定数・クラス（`Task`など） |

## 2. 引数の型・個数の不一致系（105件）

RBSのメソッドシグネチャを実装に合わせる修正が必要。

| Diagnostic ID | 件数 | 説明 |
|---|---|---|
| `Ruby::ArgumentTypeMismatch` | 58 | 引数の型が宣言と不一致 |
| `Ruby::UnexpectedPositionalArgument` | 43 | RBS宣言より多い位置引数 |
| `Ruby::MethodArityMismatch` | 32 | メソッドのアリティ（引数の数）がRBSと不一致 |
| `Ruby::UnexpectedKeywordArgument` | 2 | 予期しないキーワード引数 |
| `Ruby::InsufficientPositionalArguments` | 2 | 位置引数が足りない |

注: 1件が複数の Diagnostic ID に該当する場合があるため、合計は105件を超える。

## 3. 戻り値・ブロックの型不一致系（22件）

より細かい型修正が必要。

| Diagnostic ID | 件数 | 説明 |
|---|---|---|
| `Ruby::MethodBodyTypeMismatch` | 10 | メソッド本体の型が宣言と合わない |
| `Ruby::ReturnTypeMismatch` | 5 | 戻り値の型が不一致 |
| `Ruby::UnresolvedOverloading` | 3 | オーバーロードの解決失敗 |
| `Ruby::BlockBodyTypeMismatch` | 3 | ブロック本体の型不一致 |
| `Ruby::BlockTypeMismatch` | 2 | ブロック引数の型不一致 |
| `Ruby::UnexpectedYield` | 1 | 予期しないyield |

## 4. その他（2件）

| Diagnostic ID | 件数 | 説明 |
|---|---|---|
| `Ruby::AnnotationSyntaxError` | 2 | 型注釈のシンタックスエラー |

## 解決済み

- ~~`Ruby::NoMethod` (146件)~~ — PicoRuby固有のRBS定義の復活と型アノテーション追加で解消
- ~~`Ruby::UnannotatedEmptyCollection` (90件)~~ — 空配列・空ハッシュに `#: Type` 注釈を追加で解消

## 対処方針

1. **RBS定義の追加・修正**（134件）— `MethodParameterMismatch`, `UndeclaredMethodDefinition`, `UnknownConstant` を一掃できる。最もインパクト大
2. **引数・戻り値の型シグネチャ修正**（105件）— RBSのメソッドシグネチャを実装に合わせる
3. **戻り値・ブロック型の修正**（22件）— `MethodBodyTypeMismatch`, `ReturnTypeMismatch` 等の細かい修正
