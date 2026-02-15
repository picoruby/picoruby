# picoruby-drb-websocket 実装状況と計画

## 実装完了項目 ✅

### 1. picoruby-drbの抽象化 (Task #3)
- **目的**: TCPSocket依存をオプション化し、任意のトランスポート層をサポート
- **変更内容**:
  - `drb.rb`: プロトコル拡張ポイント (`create_socket`, `create_server`) 追加
  - `drb_tcp.rb`: TCP実装を分離、条件付き読み込み
  - `mrbgem.rake`: socket依存をオプション化 (`unless build.cc.command =~ /emcc/`)
  - `DRbMessage`: 既に抽象化済み（read/writeインターフェース）
- **結果**: 12/12テスト合格、後方互換性維持

### 2. picoruby-drb-websocket gem作成 (Task #11)
- **構造**:
  ```
  picoruby-drb-websocket/
  ├── mrbgem.rake              # 環境別依存関係
  ├── mrblib/
  │   └── drb_websocket.rb     # 全実装を1ファイルに統合
  ├── sig/
  │   └── drb_websocket.rbs
  ├── test/
  └── example/
  ```

- **重要な実装上の制約**:
  - **mrubyには`defined?`がない** → `begin/rescue NameError`で環境検出
  - **mrblib直下のファイルのみコンパイルされる** → 全コードを`drb_websocket.rb`に統合

### 3. マイコン実装 (Task #8)
- **環境検出**:
  ```ruby
  IS_MICROCONTROLLER = begin
    Net::WebSocket
    true
  rescue NameError
    false
  end
  ```

- **主要クラス**:
  - `DRb::WebSocket::Adapter`: Net::WebSocketラッパー
    - `read(n)`: バッファリング付き部分読み込み
    - `write(data)`: バイナリフレーム送信
  - `DRb::WebSocket::Server`: WebSocketサーバー
    - `DRbMessage`互換のリクエスト/レスポンス処理

- **プロトコル拡張**:
  ```ruby
  def create_socket(uri)
    if uri.start_with?("ws://") || uri.start_with?("wss://")
      WebSocket.connect(uri)
    else
      _ws_base_create_socket(uri)
    end
  end
  ```

### 4. WASM実装 (Task #9)
- **環境検出**:
  ```ruby
  IS_WASM = begin
    ::WebSocket.instance_methods.include?(:onopen)
  rescue NameError
    false
  end
  ```

- **主要クラス**:
  - `DRb::WebSocket::BrowserSocket`: ブラウザWebSocket APIラッパー
    - コールバック→同期変換（メッセージキュー）
    - `Machine.sleep(0.01)`でポーリング

- **制限**: サーバー実装なし（クライアント専用）

### 5. サンプルコード (Task #6)
- **マイコン用**:
  - `microcontroller/server.rb`: SensorServiceサーバー
  - `microcontroller/client.rb`: テストクライアント

- **CRuby用** (bundler/inline):
  ```ruby
  require 'bundler/inline'
  gemfile do
    source 'https://rubygems.org'
    gem 'drb'
    gem 'drb-websocket'
  end
  ```
  - `cruby_server.rb`: TestServiceサーバー
  - `cruby_client.rb`: 互換性テストクライアント

- **WASM用**:
  - `wasm/browser_client.html`: ブラウザUI（デモ + 実装ガイド）

### 6. ドキュメント (Task #6, #4)
- `example/README.md`: 詳細なガイド
- `sig/drb_websocket.rbs`: 型定義
- 相互運用性マトリックス、トラブルシューティング完備

### 7. ビルド設定
- `networking.gembox`に追加済み:
  ```ruby
  if vm_mruby?
    conf.gem core: 'picoruby-drb'
    conf.gem core: 'picoruby-drb-websocket'
  end
  ```

### 8. 型定義とSteep対応 (完了)
- **RBS型定義ファイル**:
  - `mrbgems/picoruby-drb/sig/drb.rbs`: DRbモジュール型定義
  - `mrbgems/picoruby-drb/sig/drb_tcp.rbs`: DRbTCPServer型定義
  - `mrbgems/picoruby-drb-websocket/sig/drb_websocket.rbs`: WebSocket拡張型定義
- **修正内容**:
  - DRbMessage: ソケット型を`untyped`に変更（WebSocket::Adapterサポート）
  - parse_druby_uri/parse_ws_uri: nil安全性の改善
  - handle_client: privateメソッドのシグネチャ追加
- **結果**: 全Steep型チェックエラー解消 ✅

### 9. Marshal Float対応 (完了)
- **問題**: `data_types_test`がfalseを返す（Float型が未サポート）
- **修正内容**:
  - `mrbgems/picoruby-marshal/mrblib/marshal.rb`にFloat型サポート追加
  - `TYPE_FLOAT = 'f'`定義
  - `dump_float`: Float→文字列→Marshal形式
  - `load_float`: Marshal形式→文字列→Float
- **結果**: 全データ型のマーシャリングが成功 ✅
  - String, Integer, Float, Array, Hash, Symbol, nil, true, false

## 完了した追加項目 ✅

### Task #7: PicoRuby相互運用テスト (完了)
**結果**:
- ✅ PicoRubyマイコン ↔ PicoRubyマイコン: **完全動作**
- ✅ 全テストケース合格（Test 1-5）
- ⚠️ Test 6 (`data_types_test`): 一部のデータ型で問題あり（調査中）

**テスト手順**:
```bash
# PicoRuby同士の通信テスト
bash mrbgems/picoruby-drb-websocket/example/test_integration.sh
```

### CRuby互換性について (方針変更)
**決定**:
- CRuby `drb-websocket` gem との互換性は**諦める**
- 理由:
  - CRuby gemは複雑なプロトコル（36バイトUUIDプレフィックス、EventMachine、バイト配列）を使用
  - PicoRubyの制約下での完全な互換性実装は困難
  - 本来の目的（マイコン ↔ ブラウザWASM）には不要

**採用したアプローチ**:
- シンプルなプロトコル: WebSocketバイナリフレームで直接DRbメッセージを送受信
- ターゲット: PicoRubyマイコン ↔ PicoRuby WASM（ブラウザ）の通信

### Task #5: ユニットテスト (未実施)
**実装内容**:
- 統合テスト (`test_drb_websocket.sh`) で基本機能は確認済み
- 詳細なユニットテストは今後実装予定

**テストファイル**: `test/drb_websocket_test.rb` (未作成)

### Task #10: ブラウザデモアプリケーション
**実装内容**:
- PicoRuby WASMビルドにpicoruby-drb-websocket統合
- ブラウザUIの完成（現在はプレースホルダー）
- 実際のマイコン↔ブラウザ通信デモ

## 技術的知見

### 重要な実装上の制約
1. **mrubyには`defined?`がない**
   - 解決: `begin/rescue NameError`で代替

2. **mrblib直下のみコンパイルされる**
   - 解決: サブディレクトリではなく、全コードを1ファイルに統合

3. **Ruby 4ではdrbが標準ライブラリでない**
   - 解決: bundler/inlineのgemfileに`gem 'drb'`を追加

### アーキテクチャの選択
- **アプローチ**: picoruby-drbの抽象化 + WebSocket拡張
  - 利点: DRbの完全な機能が使える、コード重複なし
  - 対案（別gemアプローチ）より優れている理由:
    - 保守が簡単（1つのDRb実装）
    - WebSocket以外にも拡張可能（シリアル、USBなど）

### プロトコル詳細
- **トランスポート**: WebSocketバイナリフレーム
- **メッセージフォーマット**: 4バイトヘッダ（メッセージサイズ、big-endian）+ Marshalデータ
- **互換性**: PicoRuby専用（CRuby `drb-websocket` gemとは互換性なし）
- **特徴**:
  - シンプルな実装（UUIDプレフィックスなし）
  - 各DRbメッセージフィールドが個別のWebSocketフレームとして送信
  - クライアント側でバッファリングして正しく再構成

## 既知の問題と解決策

### 問題1: ビルドでクラッシュ
**症状**: `exception corrupted (Exception)`
**原因**: クリーンビルドが必要
**解決**: `rake clean && CONFIG=microruby rake`

### 問題2: CRubyクライアントでgem読み込みエラー
**症状**: `cannot load such file -- drb/drb`
**原因**: Ruby 4でdrbが標準ライブラリでない
**解決**: gemfileに`gem 'drb'`追加済み

## 未完了項目 ⏳

### Task #10: ブラウザデモアプリケーション (次のステップ)
**実装内容**:
- PicoRuby WASMビルドにpicoruby-drb-websocket統合
- ブラウザUIの完成（現在はプレースホルダー）
- 実際のマイコン↔ブラウザ通信デモ

### Task #11: PicoRubyプロトコル対応CRuby gem作成
**目的**: PicoRuby ↔ CRuby間のWebSocket DRb通信を可能にする

**実装内容**:
- 新しいCRuby gem `picoruby-drb-websocket` を作成
  - 既存の `drb-websocket` gem とは別物（異なるプロトコル）
  - PicoRubyのシンプルなプロトコルに対応
- **プロトコル仕様**:
  - WebSocketバイナリフレーム
  - 4バイトヘッダ（メッセージサイズ、big-endian）+ Marshalデータ
  - UUIDプレフィックスなし
- **主要クラス**:
  - `PicoRuby::DRb::WebSocket::Client`: WebSocketクライアント
  - `PicoRuby::DRb::WebSocket::Server`: WebSocketサーバー
  - `PicoRuby::DRb::WebSocket::Adapter`: WebSocketラッパー
- **利用シーン**:
  - CRubyサーバー ↔ PicoRubyマイコンクライアント
  - CRubyクライアント ↔ PicoRubyマイコンサーバー
  - 開発・デバッグツール
- **依存関係**:
  - `drb` gem
  - `websocket-client-simple` または `faye-websocket`
- **公開**: RubyGems.org

**メリット**:
- PicoRubyマイコンとCRubyアプリケーション間の通信
- 既存のCRubyエコシステムとの統合
- デバッグ・開発ツールの充実

### Task #5: ユニットテスト (オプション)
**実装内容**:
- 詳細なユニットテストの作成
- テストケース:
  - Adapterバッファリング
  - URI parsing
  - 環境検出
  - 基本的なRPC

## 今後の作業手順

### 短期（次回セッション）
1. **Task #10実施**: ブラウザデモアプリケーション
   - WASM環境でのテスト
   - ブラウザUIの実装
   - マイコン↔ブラウザ通信デモ

2. **ドキュメント更新**:
   - CRuby互換性の注記を削除
   - PicoRuby専用プロトコルの説明を追加
   - 使用例の更新

### 中期
4. **Task #10**: ブラウザデモ
   - WASMビルド設定
   - ブラウザUIの完成
   - デモシナリオ作成

5. **Task #11**: PicoRubyプロトコル対応CRuby gem作成
   - gem skeltonの作成
   - WebSocketクライアント/サーバー実装
   - PicoRubyプロトコル対応
   - RubyGems.orgへの公開

6. **パフォーマンステスト**:
   - レイテンシ測定
   - スループット測定
   - メモリ使用量確認

### 長期
7. **機能拡張**:
   - wss://サーバーサポート
   - SSL/TLS対応
   - 他のトランスポート層（シリアル、USBなど）

8. **ドキュメント充実**:
   - API リファレンス
   - チュートリアル
   - ベストプラクティス

## ファイル一覧

### 実装ファイル
- `mrbgems/picoruby-drb/mrblib/drb.rb` (変更)
- `mrbgems/picoruby-drb/mrblib/drb_tcp.rb` (新規)
- `mrbgems/picoruby-drb/mrblib/drb_object.rb` (変更)
- `mrbgems/picoruby-drb/mrbgem.rake` (変更)
- `mrbgems/picoruby-drb-websocket/mrblib/drb_websocket.rb` (新規)
- `mrbgems/picoruby-drb-websocket/mrbgem.rake` (新規)
- `mrbgems/networking.gembox` (変更)

### 型定義ファイル
- `mrbgems/picoruby-drb/sig/drb.rbs` (変更)
- `mrbgems/picoruby-drb/sig/drb_tcp.rbs` (新規)
- `mrbgems/picoruby-drb-websocket/sig/drb_websocket.rbs` (新規)
- `mrbgems/picoruby-net-websocket/sig/websocket.rbs` (変更)
- `mrbgems/picoruby-net-websocket/sig/client.rbs` (変更)
- `mrbgems/picoruby-net-websocket/sig/server.rbs` (変更)

### ドキュメント
- `mrbgems/picoruby-drb-websocket/example/README.md`
- `mrbgems/picoruby-drb-websocket/IMPLEMENTATION_STATUS.md` (本ファイル)

### サンプルコード
- `mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb` (統合server/client)
- `mrbgems/picoruby-drb-websocket/example/test_integration.sh` (統合テスト)
- `mrbgems/picoruby-drb-websocket/example/wasm/browser_client.html`
- `mrbgems/picoruby-drb/example/picoruby_interop.rb` (TCP版統合server/client)
- `mrbgems/picoruby-drb/example/cruby_interop.rb` (CRuby版統合server/client)

## ビルド・テストコマンド

### ビルド
```bash
# クリーンビルド（推奨）
rake clean && CONFIG=microruby rake

# 通常ビルド
CONFIG=microruby rake
```

### テスト
```bash
# Steep型チェック
rake steep

# picoruby-drbユニットテスト
rake test:gems:microruby[picoruby-drb]

# WebSocket DRb統合テスト
bash mrbgems/picoruby-drb-websocket/example/test_integration.sh

# 手動テスト
# Terminal 1:
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb server

# Terminal 2:
build/host/bin/microruby mrbgems/picoruby-drb-websocket/example/picoruby_interop.rb client
```

## まとめ

**達成したこと**:
- ✅ picoruby-drbの完全な抽象化（トランスポート層の分離）
- ✅ WebSocketプロトコルサポート（マイコン + WASM対応）
- ✅ PicoRuby同士の通信が完全動作
- ✅ シンプルで安定したプロトコル実装
- ✅ 包括的なサンプルとドキュメント
- ✅ `DRbObject.new_with_uri` APIの追加（CRuby互換）

**方針変更**:
- ❌ CRuby `drb-websocket` gem互換性: 諦めた
  - 理由: 複雑なプロトコル、PicoRubyの制約、本来の目的に不要
- ✅ PicoRuby専用のシンプルな実装に集中

**残りの作業**:
- ⏳ ブラウザWASMデモアプリケーション
- ⏳ PicoRubyプロトコル対応CRuby gem作成
- ⏳ ユニットテスト実装（オプション）
- ⏳ ドキュメント最終調整

**次回の優先事項**:
1. Task #10（ブラウザデモ）実装
2. Task #11（CRuby gem）実装
3. マイコン↔ブラウザ通信の動作確認
4. マイコン↔CRuby通信の動作確認
5. リリース準備

## テスト結果

### 統合テスト (test_integration.sh)
```
Test 1: Hello                    ✅ PASS
Test 2: Temperature              ✅ PASS
Test 3: Counter (複数回呼び出し)  ✅ PASS
Test 4: Echo                     ✅ PASS
Test 5: Current Time             ✅ PASS
Test 6: Data Types              ✅ PASS
  - String, Integer, Float, Array, Hash, Symbol, nil, true, false

Overall: ✅ PASSED (6/6 tests)
```

### ユニットテスト (picoruby-drb)
```
✅ 12/12 tests passed
```

### Steep型チェック
```
✅ No type error detected
```

---

最終更新: 2026-02-14
作成者: Claude Code with Hasumi
