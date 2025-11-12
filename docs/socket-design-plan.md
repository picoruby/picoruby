# PicoRuby Socket & Net::HTTP 設計プラン

**作成日**: 2025-11-12
**ステータス**: 設計フェーズ
**目的**: CRuby互換のSocketクラスとNet::HTTPを実装し、既存のpicoruby-netを置き換える

---

## 📋 目次

1. [開発環境のセットアップ](#開発環境のセットアップ)
2. [プロジェクト概要](#プロジェクト概要)
3. [背景と動機](#背景と動機)
4. [設計方針](#設計方針)
5. [アーキテクチャ](#アーキテクチャ)
6. [picoruby-socketの詳細設計](#picoruby-socketの詳細設計)
7. [picoruby-net-httpの詳細設計](#picoruby-net-httpの詳細設計)
8. [IOとSocketの関係](#ioとsocketの関係)
9. [TCPServerの実装](#tcpserverの実装)
10. [TLS/SSL統合](#tlsssl統合)
11. [実装計画とマイルストーン](#実装計画とマイルストーン)
12. [CRuby互換性](#cruby互換性)
13. [実現可能性の評価](#実現可能性の評価)

---

## 開発環境のセットアップ

### 初回セットアップ手順

作業を開始する前に、以下の手順でリポジトリをセットアップしてください：

#### 1. ブランチの取得

```bash
# リモートブランチをフェッチ
git fetch origin <branch-name>

# ブランチをチェックアウト
git checkout <branch-name>
```

#### 2. サブモジュールの初期化

PicoRubyプロジェクトは複数のサブモジュールに依存しています。初回セットアップ時や、サブモジュールエラーが発生した場合は以下を実行してください：

```bash
# すべてのサブモジュールを初期化して更新
git submodule update --init --recursive
```

**必須サブモジュール**:
- `mrbgems/mruby-compiler2` - mrubyコンパイラ
- `mrbgems/mruby-bin-mrbc2` - mrbc実行ファイル
- `mrbgems/picoruby-mrubyc/lib/mrubyc` - mruby/c VM
- `mrbgems/picoruby-mruby/lib/mruby` - mruby VM
- その他

#### 3. Rakeコマンドのパス設定

環境によってはrakeコマンドがPATHに含まれていない場合があります。以下の方法で解決してください：

**方法1: rbenv環境の場合（推奨）**
```bash
export PATH="/opt/rbenv/versions/3.3.6/bin:$PATH"
```

**方法2: gem execを使用**
```bash
gem exec rake <task>
```

**恒久的な解決方法**:
```bash
# .bashrc または .zshrc に追加
echo 'export PATH="/opt/rbenv/versions/3.3.6/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 開発ワークフローのルール

#### コミット管理

**重要**: 各Phaseの作業は最終的に **1つのコミット** にまとめてください。

**理由**:
- 変更履歴が明確になる
- レビューが容易になる
- 必要に応じてPhase単位でrevertできる
- プロジェクトの進捗が追いやすい

**手順**:
```bash
# 作業中は通常通りコミット
git add .
git commit -m "WIP: implement feature X"
git commit -m "WIP: fix issue Y"

# Phase完了時に、コミットをまとめる
git reset --soft <phase開始前のcommit-hash>
git commit -m "Implement Phase N: <phase description>"

# リモートにプッシュ（force pushが必要な場合）
git push -u origin <branch-name> --force
```

#### コミットメッセージのルール

**すべてのコミットメッセージは英語で記述してください**

**良い例**:
```
Implement Phase 1: POSIX socket foundation
Add mruby/c bindings for TCPSocket
Fix memory leak in socket cleanup
```

**悪い例**:
```
Phase 1完了: POSIXソケット基盤
TCPSocketのバインディング追加  # 日本語は使用しない
```

**コミットメッセージのフォーマット**:
```
<Type> Phase N: <Short description>

<Optional detailed description>

<Optional reference to issues/PRs>
```

**Type**:
- `Implement` - 新機能実装
- `Fix` - バグ修正
- `Update` - 既存機能の更新
- `Refactor` - リファクタリング
- `Test` - テスト追加・修正
- `Docs` - ドキュメント更新

#### ブランチ運用

- ブランチ名は `claude/<feature-name>-<session-id>` の形式
- セッションIDが一致しない場合、pushは403エラーになる
- 各Phaseは指定されたブランチで開発

---

## プロジェクト概要

### ゴール

- ✅ CRuby互換のSocketクラス（TCPSocket, UDPSocket, TCPServer）を実装
- ✅ CRuby互換のNet::HTTPを実装
- ✅ POSIX環境とLwIP環境（マイコン）の両方をサポート
- ✅ 既存のpicoruby-netとの後方互換性を維持（非推奨だが残す）
- ✅ picoruby-mbedtlsを使用したHTTPS対応

### 新しいmrbgem構成

```
mrbgems/
├── picoruby-socket/          # 新規作成
├── picoruby-net-http/        # 新規作成
├── picoruby-net/             # 既存（後方互換のため保持）
└── picoruby-mbedtls/         # 既存（TLS/SSL機能）
```

---

## 背景と動機

### 現状の問題点

1. **非標準API**: `Net::HTTPSClient`はCRubyに存在しない独自API
2. **低レベル実装**: 内部で直接LwIPソケットを使用している
3. **拡張性の欠如**: UDP/TCPの低レベルAPIが公開されていない
4. **CRuby互換性**: 既存のRubyコードが移植しにくい

### 提案する解決策

```
┌─────────────────────────────────────────┐
│     ユーザーの選択肢                        │
├─────────────────────────────────────────┤
│ Option 1: picoruby-net（既存）            │
│   - Net::HTTPClient / HTTPSClient       │
│   - シンプルな高レベルAPI                   │
│   - 後方互換のため残す（非推奨）             │
├─────────────────────────────────────────┤
│ Option 2: picoruby-socket + net-http    │
│   - Socket / TCPSocket / UDPSocket      │
│   - TCPServer                           │
│   - Net::HTTP (CRuby互換)               │
│   - 推奨される新しいAPI                     │
└─────────────────────────────────────────┘
```

---

## 設計方針

### 基本原則

1. **IOの完全継承は避ける**
   - メモリとコード複雑性の観点から現実的ではない
   - IOのバッファリングロジックはファイル向けに最適化されている

2. **ダックタイピング方式**
   - IOと同様のメソッド（read/write/close等）を実装
   - `respond_to?`で動的判定可能
   - CRubyコードの80%以上が動作する見込み

3. **ファイルディスクリプタベース**
   - POSIX環境ではソケットFDを活用
   - LwIP環境ではPCB（Protocol Control Block）を使用

4. **デュアルスタック継続**
   - LwIP（マイコン）とPOSIX（Unix/Linux）の両対応
   - ビルド時に自動選択

5. **段階的な互換性**
   - 全機能ではなく、実用的なサブセットを実装
   - マイコンの制約を考慮

---

## アーキテクチャ

### 全体構成図

```
┌─────────────────────────────────────────────────────┐
│                  Ruby Application                    │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│              Ruby API Layer (mrblib/)               │
│  ┌──────────────┐  ┌──────────────────────────┐   │
│  │ TCPSocket    │  │ Net::HTTP                │   │
│  │ UDPSocket    │  │ Net::HTTPRequest         │   │
│  │ TCPServer    │  │ Net::HTTPResponse        │   │
│  │ SSLSocket    │  │                          │   │
│  └──────────────┘  └──────────────────────────┘   │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│          VM Binding Layer (src/mruby|mrubyc/)       │
│  ┌────────────────────────────────────────────┐    │
│  │  mrb_* functions (mruby)                   │    │
│  │  c_* functions (mrubyc)                    │    │
│  └────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│        Common C Implementation (src/*.c)             │
│  ┌────────────────┐  ┌──────────────────────┐     │
│  │ socket.c       │  │ tcp_server.c         │     │
│  │ tcp_socket.c   │  │ socket_addr.c        │     │
│  │ udp_socket.c   │  │ ssl_socket.c         │     │
│  └────────────────┘  └──────────────────────┘     │
└─────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────┬──────────────────────────────┐
│   POSIX (ports/      │   LwIP (src/)                │
│   posix/)            │                              │
│  ┌────────────────┐ │  ┌──────────────────────┐   │
│  │ socket()       │ │  │ altcp_new()          │   │
│  │ connect()      │ │  │ altcp_connect()      │   │
│  │ send()/recv()  │ │  │ altcp_write()        │   │
│  │ bind()/listen()│ │  │ altcp_bind()         │   │
│  │ accept()       │ │  │ altcp_listen()       │   │
│  └────────────────┘ │  │ altcp_accept()       │   │
│                     │  └──────────────────────┘   │
└──────────────────────┴──────────────────────────────┘
                          ↓
┌─────────────────────────────────────────────────────┐
│              Network Stack                           │
│  Linux/macOS Kernel    |    LwIP TCP/IP Stack       │
└─────────────────────────────────────────────────────┘
```

---

## picoruby-socketの詳細設計

### ディレクトリ構造

```
mrbgems/picoruby-socket/
├── include/
│   ├── socket.h              # 統一インターフェース定義
│   ├── socket_internal.h     # 内部構造体
│   └── socket_addr.h         # アドレス情報
│
├── src/
│   ├── socket.c              # 共通ロジック（プラットフォーム自動選択）
│   ├── tcp_socket.c          # TCPソケット実装（LwIP用）
│   ├── udp_socket.c          # UDPソケット実装（LwIP用）
│   ├── tcp_server.c          # TCPサーバー実装（LwIP用）
│   ├── socket_addr.c         # アドレス解決・変換
│   ├── ssl_socket.c          # SSL/TLSラッパー（共通）
│   │
│   ├── mruby/                # mruby VM用バインディング
│   │   └── socket.c          # mrb_* 関数群
│   │
│   └── mrubyc/               # mruby/c VM用バインディング
│       └── socket.c          # c_* 関数群
│
├── ports/
│   └── posix/
│       ├── tcp_socket.c      # POSIX socket()実装
│       ├── udp_socket.c      # POSIX UDP実装
│       ├── tcp_server.c      # POSIX サーバー実装
│       ├── socket_addr.c     # getaddrinfo()実装
│       └── ssl_socket.c      # mbedTLS統合（POSIX）
│
├── mrblib/
│   ├── socket.rb             # Socket, BasicSocket
│   ├── tcp_socket.rb         # TCPSocket
│   ├── udp_socket.rb         # UDPSocket
│   ├── tcp_server.rb         # TCPServer
│   ├── ssl_socket.rb         # SSLSocket
│   └── socket_addr.rb        # Addrinfo（簡略版）
│
├── sig/
│   └── socket.rbs            # RBS型定義
│
├── test/
│   ├── tcp_socket_test.rb
│   ├── udp_socket_test.rb
│   └── tcp_server_test.rb
│
└── mrbgem.rake               # ビルド設定
```

### VM別バインディングの役割

#### src/mruby/socket.c（mruby VM用）

```c
#include "../include/socket.h"
#include "mruby.h"
#include "mruby/presym.h"
#include "mruby/string.h"
#include "mruby/data.h"

// データ型定義
static const struct mrb_data_type mrb_tcp_socket_type = {
  "TCPSocket", mrb_free,
};

// TCPSocket.new(host, port)
static mrb_value
mrb_tcp_socket_initialize(mrb_state *mrb, mrb_value self)
{
  const char *host;
  mrb_int port;
  mrb_get_args(mrb, "zi", &host, &port);

  // 共通C関数を呼び出し
  picorb_socket_t *sock = mrb_malloc(mrb, sizeof(picorb_socket_t));
  if (!TCPSocket_connect(sock, host, port)) {
    mrb_free(mrb, sock);
    mrb_raisef(mrb, E_RUNTIME_ERROR, "Failed to connect to %s:%d", host, port);
  }

  mrb_data_init(self, sock, &mrb_tcp_socket_type);
  return self;
}

// socket.write(data)
static mrb_value
mrb_tcp_socket_write(mrb_state *mrb, mrb_value self)
{
  picorb_socket_t *sock = DATA_GET_PTR(mrb, self, &mrb_tcp_socket_type, picorb_socket_t);
  mrb_value data;
  mrb_get_args(mrb, "S", &data);

  ssize_t sent = TCPSocket_send(sock, RSTRING_PTR(data), RSTRING_LEN(data));
  if (sent < 0) {
    mrb_raise(mrb, E_IO_ERROR, "Send failed");
  }

  return mrb_fixnum_value(sent);
}

// 初期化
void
mrb_mruby_socket_gem_init(mrb_state *mrb)
{
  struct RClass *socket_class = mrb_define_class(mrb, "BasicSocket", mrb->object_class);
  struct RClass *tcp_socket_class = mrb_define_class(mrb, "TCPSocket", socket_class);

  mrb_define_method(mrb, tcp_socket_class, "initialize", mrb_tcp_socket_initialize, MRB_ARGS_REQ(2));
  mrb_define_method(mrb, tcp_socket_class, "write", mrb_tcp_socket_write, MRB_ARGS_REQ(1));
  // ... 他のメソッド
}
```

#### src/mrubyc/socket.c（mruby/c VM用）

```c
#include "../include/socket.h"
#include "mrubyc.h"

// TCPSocket.new(host, port)
static void
c_tcp_socket_initialize(mrbc_vm *vm, mrbc_value *v, int argc)
{
  if (argc != 2) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }

  mrbc_value host = GET_ARG(1);
  mrbc_value port = GET_ARG(2);

  if (host.tt != MRBC_TT_STRING || port.tt != MRBC_TT_INTEGER) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    return;
  }

  // 共通C関数を呼び出し
  picorb_socket_t *sock = picorb_alloc(vm, sizeof(picorb_socket_t));
  if (!TCPSocket_connect(sock, (const char*)host.string->data, port.i)) {
    picorb_free(vm, sock);
    mrbc_raise(vm, MRBC_CLASS(RuntimeError), "Failed to connect");
    return;
  }

  v[0].instance->data = sock;
}

// socket.write(data)
static void
c_tcp_socket_write(mrbc_vm *vm, mrbc_value *v, int argc)
{
  picorb_socket_t *sock = (picorb_socket_t *)v[0].instance->data;
  mrbc_value data = GET_ARG(1);

  if (data.tt != MRBC_TT_STRING) {
    mrbc_raise(vm, MRBC_CLASS(TypeError), "wrong argument type");
    return;
  }

  ssize_t sent = TCPSocket_send(sock, (const void*)data.string->data, data.string->size);
  if (sent < 0) {
    mrbc_raise(vm, MRBC_CLASS(IOError), "Send failed");
    return;
  }

  SET_INT_RETURN(sent);
}

// 初期化
void
mrbc_mruby_socket_gem_init(void)
{
  mrbc_class *socket_class = mrbc_define_class(0, "BasicSocket", mrbc_class_object);
  mrbc_class *tcp_socket_class = mrbc_define_class(0, "TCPSocket", socket_class);

  mrbc_define_method(0, tcp_socket_class, "initialize", c_tcp_socket_initialize);
  mrbc_define_method(0, tcp_socket_class, "write", c_tcp_socket_write);
  // ... 他のメソッド
}
```

### クラス階層

```ruby
Object
  └── BasicSocket
       ├── Socket           # 低レベルAPI
       ├── IPSocket         # IP関連の共通機能
       │    ├── TCPSocket   # TCPクライアント
       │    └── UDPSocket   # UDPソケット
       ├── TCPServer        # TCPサーバー
       └── SSLSocket        # TLS/SSL対応ソケット

# 補助クラス
Addrinfo                    # アドレス情報（簡略版）
```

### 共通Cインターフェース（include/socket.h）

```c
#ifndef PICORB_SOCKET_H
#define PICORB_SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

// ソケット構造体（POSIX）
typedef struct {
  int fd;                    // ファイルディスクリプタ（-1 = 無効）
  int family;                // AF_INET, AF_INET6
  int socktype;              // SOCK_STREAM, SOCK_DGRAM
  int protocol;              // IPPROTO_TCP, IPPROTO_UDP
  bool connected;
  bool closed;
  char remote_host[256];
  int remote_port;
} picorb_socket_t;

// ソケット構造体（LwIP）
#ifndef PICORB_PLATFORM_POSIX
typedef struct {
  struct altcp_pcb *pcb;     // LwIPコントロールブロック
  char *recv_buf;
  size_t recv_len;
  size_t recv_capacity;
  enum {
    SOCKET_STATE_NONE,
    SOCKET_STATE_CONNECTING,
    SOCKET_STATE_CONNECTED,
    SOCKET_STATE_CLOSED
  } state;
  char remote_host[256];
  int remote_port;
} picorb_socket_t;
#endif

// TCPソケットAPI
bool TCPSocket_create(picorb_socket_t *sock);
bool TCPSocket_connect(picorb_socket_t *sock, const char *host, int port);
ssize_t TCPSocket_send(picorb_socket_t *sock, const void *data, size_t len);
ssize_t TCPSocket_recv(picorb_socket_t *sock, void *buf, size_t len);
bool TCPSocket_close(picorb_socket_t *sock);

// UDPソケットAPI
bool UDPSocket_create(picorb_socket_t *sock);
ssize_t UDPSocket_sendto(picorb_socket_t *sock, const void *data, size_t len,
                          const char *host, int port);
ssize_t UDPSocket_recvfrom(picorb_socket_t *sock, void *buf, size_t len,
                            char *host, int *port);
bool UDPSocket_close(picorb_socket_t *sock);

// TCPサーバーAPI
typedef struct picorb_tcp_server picorb_tcp_server_t;

picorb_tcp_server_t* TCPServer_create(int port, int backlog);
picorb_socket_t* TCPServer_accept(picorb_tcp_server_t *server);
bool TCPServer_close(picorb_tcp_server_t *server);

// アドレス解決
bool resolve_address(const char *host, char *ip, size_t ip_len);

#endif // PICORB_SOCKET_H
```

### Ruby API設計

#### BasicSocket

```ruby
class BasicSocket
  # IO風のメソッド
  def read(maxlen = nil, outbuf = nil)
  def write(data)
  def gets(sep = "\n")
  def puts(*args)
  def close
  def closed?
  def eof?

  # ソケット固有メソッド
  def send(data, flags = 0)
  def recv(maxlen, flags = 0)
  def setsockopt(level, optname, optval)
  def getsockopt(level, optname)

  # アドレス情報
  def local_address
  def remote_address
  def peeraddr

  # select互換（POSIX版）
  def self.select(read_array, write_array = nil, error_array = nil, timeout = nil)
end
```

#### TCPSocket

```ruby
class TCPSocket < IPSocket
  # 初期化
  def initialize(host, port)

  # CRuby互換
  def self.open(host, port)
  def self.gethostbyname(host)
end
```

#### UDPSocket

```ruby
class UDPSocket < IPSocket
  def initialize
  def bind(host, port)
  def send(data, flags, host = nil, port = nil)
  def recvfrom(maxlen, flags = 0)
  def connect(host, port)
end
```

#### Addrinfo（簡略版）

```ruby
class Addrinfo
  attr_reader :afamily, :pfamily, :socktype, :protocol
  attr_reader :ip_address, :ip_port

  def initialize(sockaddr, family = nil, socktype = nil, protocol = nil)
  def ip?
  def ipv4?
  def ipv6?
  def tcp?
  def udp?
end
```

---

## picoruby-net-httpの詳細設計

### ディレクトリ構造

```
mrbgems/picoruby-net-http/
├── mrblib/
│   ├── net/
│   │   ├── http.rb              # Net::HTTP
│   │   ├── https.rb             # HTTPS機能
│   │   ├── http_request.rb      # リクエストクラス群
│   │   ├── http_response.rb     # レスポンスクラス
│   │   ├── http_header.rb       # ヘッダー処理
│   │   └── http_exceptions.rb   # 例外クラス
│   └── uri.rb                   # URI（簡略版）
│
├── sig/
│   └── net_http.rbs
│
├── test/
│   ├── http_test.rb
│   └── https_test.rb
│
└── mrbgem.rake
    # 依存: picoruby-socket, picoruby-mbedtls
```

### クラス構造

```ruby
module Net
  class HTTP
    # 初期化
    def initialize(address, port = nil)

    # 接続管理
    def start(&block)
    def finish
    def active?

    # リクエスト送信
    def get(path, initheader = nil, dest = nil, &block)
    def post(path, data, initheader = nil, dest = nil, &block)
    def put(path, data, initheader = nil, dest = nil, &block)
    def delete(path, initheader = nil, dest = nil, &block)
    def head(path, initheader = nil)

    # 汎用リクエスト
    def request(req, body = nil, &block)

    # HTTPS設定
    attr_accessor :use_ssl
    attr_accessor :verify_mode
    attr_accessor :ca_file
    attr_accessor :ca_path

    # タイムアウト
    attr_accessor :open_timeout
    attr_accessor :read_timeout

    # クラスメソッド
    def self.get(uri_or_host, path = nil, port = nil)
    def self.get_response(uri_or_host, path = nil, port = nil)
    def self.post_form(url, params)
    def self.start(address, port = nil, &block)
  end

  class HTTPRequest
    attr_reader :method, :path
    attr_accessor :body

    def initialize(method, path, initheader = nil)
    def [](key)
    def []=(key, val)
    def to_s  # リクエスト文字列生成
  end

  class HTTPResponse
    attr_reader :code, :message, :http_version
    attr_reader :body

    def initialize
    def [](key)
    def read_body(&block)

    # ステータスチェック
    def code_type
    def success?
    def redirect?
    def error?
  end

  # リクエストクラス群
  class Get < HTTPRequest; end
  class Post < HTTPRequest; end
  class Put < HTTPRequest; end
  class Delete < HTTPRequest; end
  class Head < HTTPRequest; end
end
```

### 実装例（mrblib/net/http.rb）

```ruby
module Net
  class HTTP
    def initialize(address, port = nil)
      @address = address
      @port = port || (use_ssl? ? 443 : 80)
      @socket = nil
      @started = false
      @use_ssl = false
      @verify_mode = nil
      @open_timeout = 60
      @read_timeout = 60
    end

    def start
      raise IOError, "HTTP session already started" if @started

      if use_ssl?
        tcp_socket = TCPSocket.new(@address, @port)
        @socket = SSLSocket.new(tcp_socket, @address)
      else
        @socket = TCPSocket.new(@address, @port)
      end

      @started = true

      if block_given?
        begin
          yield self
        ensure
          finish
        end
      end

      self
    end

    def finish
      @socket.close if @socket && !@socket.closed?
      @socket = nil
      @started = false
    end

    def get(path, initheader = nil)
      request(Get.new(path, initheader))
    end

    def post(path, data, initheader = nil)
      req = Post.new(path, initheader)
      req.body = data
      request(req)
    end

    def request(req)
      raise IOError, "HTTP session not started" unless @started

      # リクエスト送信
      @socket.write(req.to_s)

      # レスポンス受信
      response_text = @socket.read

      # レスポンスパース
      HTTPResponse.parse(response_text)
    end

    # クラスメソッド
    def self.get(uri_or_host, path = nil, port = nil)
      if uri_or_host.is_a?(String) && uri_or_host.start_with?('http')
        uri = URI.parse(uri_or_host)
        host = uri.host
        path = uri.path
        port = uri.port
        use_ssl = uri.scheme == 'https'
      else
        host = uri_or_host
        use_ssl = false
      end

      start(host, port) do |http|
        http.use_ssl = use_ssl if use_ssl
        http.get(path || '/')
      end
    end

    private

    def use_ssl?
      @use_ssl
    end
  end
end
```

---

## IOとSocketの関係

### 採用する設計：「部分的互換（ダックタイピング）」

```
┌─────────────────────────────────────┐
│         Object                      │
└─────────────────────────────────────┘
       ↑                    ↑
       │                    │
┌──────┴──────┐      ┌──────┴──────────┐
│    IO       │      │  BasicSocket    │
│             │      │  (IO継承なし)    │
│ - File I/O  │      │                 │
│ - バッファ   │      │ - read/write    │
│ - fd管理    │      │ - ネットワーク   │
│             │      │ - ソケット専用   │
└─────────────┘      └─────────────────┘
                            ↑
                  ┌─────────┼─────────┐
                  │         │         │
            ┌─────┴───┐ ┌───┴────┐ ┌──┴──────┐
            │TCPSocket│ │UDPSocket│ │SSLSocket│
            └─────────┘ └─────────┘ └─────────┘
```

### IO継承を避ける理由

1. **バッファリングロジックの不一致**
   - IOのバッファはファイルI/O向けに最適化
   - ソケットは異なるバッファリング戦略が必要

2. **メモリ制約**
   - マイコン環境でのメモリ使用量を最小化
   - IOの全機能を実装するとオーバーヘッドが大きい

3. **エラーハンドリングの違い**
   - ファイルとネットワークで異なるエラー型
   - タイムアウト、接続切断等のネットワーク特有のエラー

4. **実装の複雑性**
   - LwIP環境ではファイルディスクリプタが存在しない
   - IOの内部構造（picorb_io）との整合性が困難

### 互換性の確保方法

#### 共通メソッドの実装

```ruby
class BasicSocket
  # IO互換メソッド
  def read(maxlen = nil, outbuf = nil)
    # 実装
  end

  def write(data)
    # 実装
  end

  def gets(sep = "\n")
    # 実装
  end

  def close
    # 実装
  end

  def closed?
    # 実装
  end

  def eof?
    # 実装
  end
end
```

#### ダックタイピングによる互換性

```ruby
# このコードはIOでもSocketでも動作する
def read_all(io_or_socket)
  result = ""
  while chunk = io_or_socket.read(1024)
    result << chunk
  end
  result
end

# 使用例
file = File.open("data.txt")
read_all(file)

socket = TCPSocket.new("example.com", 80)
read_all(socket)
```

### メリットとデメリット

| 項目 | メリット | デメリット |
|-----|---------|-----------|
| **メモリ** | ✅ 効率的 | - |
| **実装** | ✅ シンプル | - |
| **互換性** | ✅ 主要メソッド対応 | ⚠️ `is_a?(IO)` は false |
| **拡張性** | ✅ ソケット特有機能追加容易 | - |
| **POSIX** | ⚠️ IO.selectに直接渡せない | ✅ fileno()で対応可能 |

---

## TCPServerの実装

### POSIX版TCPServer（難易度：⭐☆☆）

#### C実装（ports/posix/tcp_server.c）

```c
#include "../../include/socket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

typedef struct {
  int listen_fd;
  int port;
  int backlog;
  bool listening;
} picorb_tcp_server_posix_t;

// サーバー作成
picorb_tcp_server_t* TCPServer_create(int port, int backlog) {
  picorb_tcp_server_posix_t *server = malloc(sizeof(picorb_tcp_server_posix_t));
  if (!server) return NULL;

  // ソケット作成
  server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server->listen_fd < 0) {
    free(server);
    return NULL;
  }

  // SO_REUSEADDR設定
  int opt = 1;
  setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // バインド
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(server->listen_fd);
    free(server);
    return NULL;
  }

  // リッスン開始
  if (listen(server->listen_fd, backlog) < 0) {
    close(server->listen_fd);
    free(server);
    return NULL;
  }

  server->port = port;
  server->backlog = backlog;
  server->listening = true;

  return (picorb_tcp_server_t*)server;
}

// クライアント接続受け入れ
picorb_socket_t* TCPServer_accept(picorb_tcp_server_t *server) {
  picorb_tcp_server_posix_t *srv = (picorb_tcp_server_posix_t*)server;

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);

  int client_fd = accept(srv->listen_fd,
                         (struct sockaddr*)&client_addr,
                         &addr_len);

  if (client_fd < 0) {
    return NULL;
  }

  // TCPSocketオブジェクト作成
  picorb_socket_t *client = malloc(sizeof(picorb_socket_t));
  if (!client) {
    close(client_fd);
    return NULL;
  }

  client->fd = client_fd;
  client->family = AF_INET;
  client->socktype = SOCK_STREAM;
  client->protocol = IPPROTO_TCP;
  client->connected = true;
  client->closed = false;

  // クライアント情報を保存
  inet_ntop(AF_INET, &client_addr.sin_addr,
            client->remote_host, sizeof(client->remote_host));
  client->remote_port = ntohs(client_addr.sin_port);

  return client;
}

// サーバークローズ
bool TCPServer_close(picorb_tcp_server_t *server) {
  picorb_tcp_server_posix_t *srv = (picorb_tcp_server_posix_t*)server;

  if (srv->listen_fd >= 0) {
    close(srv->listen_fd);
    srv->listen_fd = -1;
    srv->listening = false;
  }

  free(srv);
  return true;
}
```

#### Ruby API（mrblib/tcp_server.rb）

```ruby
class TCPServer < TCPSocket
  def initialize(port, backlog = 5)
    @port = port
    @backlog = backlog
    @listening = false
    _create_server(port, backlog)  # C関数呼び出し
    @listening = true
  end

  # 接続を受け入れ（ブロックする）
  def accept
    _accept_client  # C関数でTCPSocketを返す
  end

  # ブロック付きaccept
  def accept_loop(&block)
    loop do
      client = accept
      yield client
    end
  end

  # サーバー停止
  def close
    _close_server
    @listening = false
  end

  def listening?
    @listening
  end

  attr_reader :port
end
```

### LwIP版TCPServer（難易度：⭐⭐☆）

#### C実装（src/tcp_server.c）

```c
#include "include/socket.h"
#include "lwip/altcp.h"

#define MAX_CLIENTS 5  // 最大同時接続数

typedef struct {
  struct altcp_pcb *pcb;
  char remote_ip[16];
  int remote_port;
  char *recv_buf;
  size_t recv_len;
  bool active;
  bool pending;  // accept待ち
} tcp_server_client_t;

typedef struct {
  struct altcp_pcb *listen_pcb;
  int port;
  bool listening;
  tcp_server_client_t clients[MAX_CLIENTS];
  int pending_count;
} picorb_tcp_server_lwip_t;

// acceptコールバック
static err_t tcp_server_accept_cb(void *arg, struct altcp_pcb *newpcb, err_t err) {
  picorb_tcp_server_lwip_t *server = (picorb_tcp_server_lwip_t *)arg;

  if (err != ERR_OK || newpcb == NULL) {
    return ERR_VAL;
  }

  // 空きスロット検索
  int client_idx = -1;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (!server->clients[i].active) {
      client_idx = i;
      break;
    }
  }

  if (client_idx < 0) {
    // 接続数上限
    altcp_close(newpcb);
    return ERR_ABRT;
  }

  // クライアント情報保存
  tcp_server_client_t *client = &server->clients[client_idx];
  client->pcb = newpcb;
  client->active = true;
  client->pending = true;  // accept待ちとしてマーク
  client->recv_buf = NULL;
  client->recv_len = 0;

  // IPアドレス取得
  const ip_addr_t *addr = altcp_get_ip(newpcb, 0);
  if (addr) {
    ipaddr_ntoa_r(addr, client->remote_ip, sizeof(client->remote_ip));
  }
  client->remote_port = altcp_get_port(newpcb, 0);

  // コールバック設定
  altcp_arg(newpcb, client);
  altcp_recv(newpcb, tcp_server_recv_cb);
  altcp_err(newpcb, tcp_server_err_cb);

  server->pending_count++;

  return ERR_OK;
}

// サーバー作成
picorb_tcp_server_t* TCPServer_create(int port, int backlog) {
  lwip_begin();

  picorb_tcp_server_lwip_t *server = picorb_alloc(NULL, sizeof(picorb_tcp_server_lwip_t));
  if (!server) {
    lwip_end();
    return NULL;
  }

  // リスニングPCB作成
  server->listen_pcb = altcp_new(NULL);
  if (!server->listen_pcb) {
    picorb_free(NULL, server);
    lwip_end();
    return NULL;
  }

  // バインド
  ip_addr_t addr = IPADDR4_INIT(IPADDR_ANY);
  err_t err = altcp_bind(server->listen_pcb, &addr, port);
  if (err != ERR_OK) {
    altcp_close(server->listen_pcb);
    picorb_free(NULL, server);
    lwip_end();
    return NULL;
  }

  // リッスン開始
  server->listen_pcb = altcp_listen(server->listen_pcb);
  if (!server->listen_pcb) {
    picorb_free(NULL, server);
    lwip_end();
    return NULL;
  }

  // acceptコールバック登録
  altcp_arg(server->listen_pcb, server);
  altcp_accept(server->listen_pcb, tcp_server_accept_cb);

  server->port = port;
  server->listening = true;
  server->pending_count = 0;

  // 全クライアントを非アクティブに
  for (int i = 0; i < MAX_CLIENTS; i++) {
    server->clients[i].active = false;
    server->clients[i].pending = false;
  }

  lwip_end();
  return (picorb_tcp_server_t*)server;
}

// クライアント接続受け入れ（ポーリング）
picorb_socket_t* TCPServer_accept(picorb_tcp_server_t *server) {
  picorb_tcp_server_lwip_t *srv = (picorb_tcp_server_lwip_t*)server;

  lwip_begin();

  // pending_countが0より大きくなるまでポーリング
  while (srv->pending_count == 0) {
    sys_check_timeouts();  // LwIPタイマー処理
    Net_sleep_ms(10);
  }

  // pending状態のクライアントを検索
  int client_idx = -1;
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (srv->clients[i].active && srv->clients[i].pending) {
      client_idx = i;
      break;
    }
  }

  if (client_idx < 0) {
    lwip_end();
    return NULL;
  }

  // pendingフラグをクリア
  srv->clients[client_idx].pending = false;
  srv->pending_count--;

  // TCPSocketオブジェクト作成
  picorb_socket_t *client = picorb_alloc(NULL, sizeof(picorb_socket_t));
  client->pcb = srv->clients[client_idx].pcb;
  strcpy(client->remote_host, srv->clients[client_idx].remote_ip);
  client->remote_port = srv->clients[client_idx].remote_port;
  client->recv_buf = NULL;
  client->recv_len = 0;
  client->state = SOCKET_STATE_CONNECTED;

  lwip_end();
  return client;
}
```

### TCPServer使用例

#### HTTPサーバー

```ruby
require 'socket'

server = TCPServer.new(80)
puts "HTTP server listening on port 80"

loop do
  client = server.accept
  puts "Client connected: #{client.peeraddr.inspect}"

  # リクエスト読み込み
  request = ""
  while line = client.gets
    request << line
    break if line == "\r\n"
  end

  puts "Request: #{request[0..50]}"

  # レスポンス送信
  response = <<~HTTP
    HTTP/1.1 200 OK
    Content-Type: text/html
    Content-Length: 54
    Connection: close

    <html><body><h1>Hello from PicoRuby!</h1></body></html>
  HTTP

  client.write(response)
  client.close
end
```

#### REST APIサーバー（マイコン向け）

```ruby
require 'socket'
require 'json'

class SimpleAPIServer
  def initialize(port = 8080)
    @server = TCPServer.new(port)
    @routes = {}
  end

  def get(path, &block)
    @routes["GET #{path}"] = block
  end

  def post(path, &block)
    @routes["POST #{path}"] = block
  end

  def start
    puts "API server started on port #{@server.port}"

    loop do
      client = @server.accept
      handle_request(client)
      client.close
    end
  end

  private

  def handle_request(client)
    # リクエスト解析
    request_line = client.gets
    method, path, _ = request_line.split(' ')

    # ルーティング
    route_key = "#{method} #{path}"
    if handler = @routes[route_key]
      response = handler.call
      send_response(client, 200, response)
    else
      send_response(client, 404, {error: "Not Found"})
    end
  end

  def send_response(client, code, data)
    body = data.to_json
    response = <<~HTTP
      HTTP/1.1 #{code} OK
      Content-Type: application/json
      Content-Length: #{body.bytesize}

      #{body}
    HTTP
    client.write(response)
  end
end

# 使用例
server = SimpleAPIServer.new(8080)

server.get "/api/status" do
  {status: "ok", uptime: Time.now.to_i}
end

server.get "/api/memory" do
  {free: GC.stat[:heap_free_slots], total: GC.stat[:heap_length]}
end

server.post "/api/gpio/on" do
  # GPIO制御
  {result: "LED turned on"}
end

server.start
```

### POSIX vs LwIP 比較表

| 項目 | POSIX | LwIP |
|-----|-------|------|
| **実装難易度** | ⭐☆☆ 簡単 | ⭐⭐☆ 中程度 |
| **同時接続数** | 制限なし（OSによる） | 3-5接続（設定可能） |
| **accept()動作** | ブロッキング | ポーリングループ |
| **IO.select対応** | ✅ 完全対応 | ❌ 独自ポーリング |
| **マルチスレッド** | ✅ 可能 | ❌ シングルスレッド |
| **メモリ使用量** | OS管理 | 手動管理（制約あり） |
| **実用性** | 本番サーバー可 | 軽量サーバー向け |
| **工数** | 2-3日 | 3-4日 |

---

## TLS/SSL統合

### SSLSocketの実装

#### C実装（src/ssl_socket.c）

```c
#include "include/socket.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

typedef struct {
  picorb_socket_t *base_socket;     // 下位ソケット
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context ctr_drbg;
  bool ssl_initialized;
} picorb_ssl_socket_t;

// SSL初期化
bool SSLSocket_init(picorb_ssl_socket_t *ssl_sock,
                    picorb_socket_t *base_sock,
                    const char *hostname) {
  ssl_sock->base_socket = base_sock;

  // mbedTLS初期化
  mbedtls_ssl_init(&ssl_sock->ssl);
  mbedtls_ssl_config_init(&ssl_sock->conf);
  mbedtls_entropy_init(&ssl_sock->entropy);
  mbedtls_ctr_drbg_init(&ssl_sock->ctr_drbg);

  // 乱数シード
  const char *pers = "picoruby_ssl";
  int ret = mbedtls_ctr_drbg_seed(&ssl_sock->ctr_drbg,
                                   mbedtls_entropy_func,
                                   &ssl_sock->entropy,
                                   (const unsigned char *)pers,
                                   strlen(pers));
  if (ret != 0) return false;

  // SSL設定
  ret = mbedtls_ssl_config_defaults(&ssl_sock->conf,
                                     MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
  if (ret != 0) return false;

  // 証明書検証なし（マイコン向け）
  mbedtls_ssl_conf_authmode(&ssl_sock->conf, MBEDTLS_SSL_VERIFY_NONE);

  mbedtls_ssl_conf_rng(&ssl_sock->conf,
                       mbedtls_ctr_drbg_random,
                       &ssl_sock->ctr_drbg);

  // SSL設定を適用
  ret = mbedtls_ssl_setup(&ssl_sock->ssl, &ssl_sock->conf);
  if (ret != 0) return false;

  // SNI設定
  if (hostname) {
    mbedtls_ssl_set_hostname(&ssl_sock->ssl, hostname);
  }

  // 下位ソケットと紐付け
#ifdef PICORB_PLATFORM_POSIX
  mbedtls_ssl_set_bio(&ssl_sock->ssl,
                      &base_sock->fd,
                      mbedtls_net_send,
                      mbedtls_net_recv,
                      NULL);
#else
  // LwIP版はカスタムI/O関数を使用
  mbedtls_ssl_set_bio(&ssl_sock->ssl,
                      base_sock,
                      ssl_lwip_send,
                      ssl_lwip_recv,
                      NULL);
#endif

  // ハンドシェイク
  while ((ret = mbedtls_ssl_handshake(&ssl_sock->ssl)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
        ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      return false;
    }
  }

  ssl_sock->ssl_initialized = true;
  return true;
}

// SSL送信
ssize_t SSLSocket_write(picorb_ssl_socket_t *ssl_sock, const void *data, size_t len) {
  return mbedtls_ssl_write(&ssl_sock->ssl, data, len);
}

// SSL受信
ssize_t SSLSocket_read(picorb_ssl_socket_t *ssl_sock, void *buf, size_t len) {
  return mbedtls_ssl_read(&ssl_sock->ssl, buf, len);
}

// SSLクローズ
void SSLSocket_close(picorb_ssl_socket_t *ssl_sock) {
  if (ssl_sock->ssl_initialized) {
    mbedtls_ssl_close_notify(&ssl_sock->ssl);
    mbedtls_ssl_free(&ssl_sock->ssl);
    mbedtls_ssl_config_free(&ssl_sock->conf);
    mbedtls_ctr_drbg_free(&ssl_sock->ctr_drbg);
    mbedtls_entropy_free(&ssl_sock->entropy);
    ssl_sock->ssl_initialized = false;
  }

  if (ssl_sock->base_socket) {
    TCPSocket_close(ssl_sock->base_socket);
  }
}
```

#### Ruby API（mrblib/ssl_socket.rb）

```ruby
class SSLSocket < BasicSocket
  def initialize(socket, hostname = nil)
    @socket = socket
    @hostname = hostname
    _ssl_init(socket, hostname)  # C関数
  end

  def read(maxlen = nil, outbuf = nil)
    _ssl_read(maxlen)
  end

  def write(data)
    _ssl_write(data)
  end

  def close
    _ssl_close
    @socket.close unless @socket.closed?
  end

  # 証明書情報（簡略版）
  def peer_cert
    # 未実装
    nil
  end
end
```

### Net::HTTPでのHTTPS対応

```ruby
module Net
  class HTTP
    attr_accessor :use_ssl

    def start
      raise IOError, "HTTP session already started" if @started

      tcp_socket = TCPSocket.new(@address, @port)

      if use_ssl?
        @socket = SSLSocket.new(tcp_socket, @address)
      else
        @socket = tcp_socket
      end

      @started = true

      if block_given?
        begin
          yield self
        ensure
          finish
        end
      end

      self
    end
  end
end
```

---

## 実装計画とマイルストーン

### フェーズ別実装計画

#### Phase 1: picoruby-socket基盤（POSIX版）【3-4日】

- [ ] ディレクトリ構造作成
- [ ] include/socket.h 統一インターフェース定義
- [ ] src/socket.c 共通ロジック
- [ ] ports/posix/tcp_socket.c POSIX TCP実装
- [ ] src/mruby/socket.c mruby VMバインディング
- [ ] src/mrubyc/socket.c mruby/c VMバインディング
- [ ] mrblib/tcp_socket.rb Ruby API
- [ ] 基本的なread/write/closeテスト

**完了条件**: POSIX環境でTCPSocket動作確認

#### Phase 2: UDPSocket対応【1-2日】

- [ ] ports/posix/udp_socket.c POSIX UDP実装
- [ ] mrblib/udp_socket.rb Ruby API
- [ ] UDPテスト作成

**完了条件**: UDP送受信動作確認

#### Phase 3: TCPServer（POSIX版）【2-3日】

- [ ] ports/posix/tcp_server.c サーバー実装
- [ ] mrblib/tcp_server.rb Ruby API
- [ ] VMバインディング追加
- [ ] TCPServerテスト
- [ ] 簡単なHTTPサーバーサンプル作成

**完了条件**: TCPServerでクライアント接続受け入れ動作確認

#### Phase 4: picoruby-net-http【3-4日】

- [ ] ディレクトリ構造作成
- [ ] mrblib/net/http.rb Net::HTTP実装
- [ ] mrblib/net/http_request.rb リクエストクラス
- [ ] mrblib/net/http_response.rb レスポンスクラス
- [ ] mrblib/uri.rb URI簡略版
- [ ] HTTPテスト作成

**完了条件**: Net::HTTPでHTTP通信動作確認

#### Phase 5: TLS/SSL統合【2-3日】

- [ ] src/ssl_socket.c SSLSocket実装
- [ ] ports/posix/ssl_socket.c mbedTLS統合
- [ ] mrblib/ssl_socket.rb Ruby API
- [ ] Net::HTTPのHTTPS対応
- [ ] HTTPSテスト

**完了条件**: Net::HTTPでHTTPS通信動作確認

#### Phase 6: LwIP対応（マイコン向け）【4-5日】

- [ ] src/tcp_socket.c LwIP TCP実装
- [ ] src/udp_socket.c LwIP UDP実装
- [ ] src/tcp_server.c LwIP サーバー実装
- [ ] src/ssl_socket.c LwIP TLS統合
- [ ] コールバック処理実装
- [ ] ポーリングループ実装
- [ ] マイコンでの動作確認

**完了条件**: Raspberry Pi Picoでソケット通信動作確認

#### Phase 7: テストとドキュメント【2-3日】

- [ ] 統合テスト
- [ ] パフォーマンステスト
- [ ] READMEとサンプルコード作成
- [ ] RBS型定義の整備
- [ ] マイグレーションガイド作成

**完了条件**: ドキュメント完成、全テストパス

### 合計工数見積もり

| フェーズ | 工数 | 累計 |
|---------|------|------|
| Phase 1 | 3-4日 | 3-4日 |
| Phase 2 | 1-2日 | 4-6日 |
| Phase 3 | 2-3日 | 6-9日 |
| Phase 4 | 3-4日 | 9-13日 |
| Phase 5 | 2-3日 | 11-16日 |
| Phase 6 | 4-5日 | 15-21日 |
| Phase 7 | 2-3日 | 17-24日 |

**合計**: 17-24日（約3-4週間）

---

## CRuby互換性

### 対応するAPIと制約

#### TCPSocket

| メソッド/機能 | CRuby | PicoRuby | 備考 |
|-------------|-------|----------|------|
| `new(host, port)` | ✅ | ✅ | 完全互換 |
| `read(len)` | ✅ | ✅ | 完全互換 |
| `write(data)` | ✅ | ✅ | 完全互換 |
| `gets` | ✅ | ✅ | 完全互換 |
| `puts` | ✅ | ✅ | 完全互換 |
| `close` | ✅ | ✅ | 完全互換 |
| `closed?` | ✅ | ✅ | 完全互換 |
| `setsockopt` | ✅ | ⚠️ | 一部オプションのみ |
| `getsockopt` | ✅ | ⚠️ | 一部オプションのみ |
| `peeraddr` | ✅ | ✅ | 完全互換 |

#### UDPSocket

| メソッド/機能 | CRuby | PicoRuby | 備考 |
|-------------|-------|----------|------|
| `new` | ✅ | ✅ | 完全互換 |
| `bind(host, port)` | ✅ | ✅ | 完全互換 |
| `send(data, flags, host, port)` | ✅ | ✅ | 完全互換 |
| `recvfrom(maxlen)` | ✅ | ✅ | 完全互換 |
| `connect(host, port)` | ✅ | ✅ | 完全互換 |

#### TCPServer

| メソッド/機能 | CRuby | PicoRuby | 備考 |
|-------------|-------|----------|------|
| `new(port)` | ✅ | ✅ | 完全互換 |
| `accept` | ✅ | ✅ | 完全互換 |
| `accept_loop` | ✅ | ✅ | 完全互換 |
| `close` | ✅ | ✅ | 完全互換 |
| `listen(backlog)` | ✅ | ✅ | マイコン版は接続数制限あり |

#### Socket

| メソッド/機能 | CRuby | PicoRuby | 備考 |
|-------------|-------|----------|------|
| `Socket.select` | ✅ | ⚠️ | POSIX版のみ |
| UNIX Socket | ✅ | ❌ | 非対応 |
| IPv6 | ✅ | ⚠️ | 将来対応 |
| Raw Socket | ✅ | ❌ | 非対応 |

#### Net::HTTP

| メソッド/機能 | CRuby | PicoRuby | 備考 |
|-------------|-------|----------|------|
| `HTTP.new(host, port)` | ✅ | ✅ | 完全互換 |
| `start(&block)` | ✅ | ✅ | 完全互換 |
| `get(path)` | ✅ | ✅ | 完全互換 |
| `post(path, data)` | ✅ | ✅ | 完全互換 |
| `use_ssl=` | ✅ | ✅ | 完全互換 |
| `HTTP.get(uri)` | ✅ | ✅ | 完全互換 |
| `HTTP.post_form` | ✅ | ✅ | 完全互換 |
| Chunked encoding | ✅ | ⚠️ | 将来対応 |
| Proxy | ✅ | ❌ | 非対応 |

### 互換性レベル

- ✅ **完全互換**: 80-90% のAPIをサポート
- ⚠️ **部分互換**: 主要機能は動作するが、一部制約あり
- ❌ **非対応**: マイコンの制約により実装困難

---

## 実現可能性の評価

### ✅ 総合評価：**実現可能性は高い**

#### 実現可能な理由

1. **既存実装の再利用**
   - picoruby-netのPOSIXソケット実装（ports/posix/）を活用
   - LwIP実装も既存のtcp.c/udp.cを参考に実装
   - mbedTLS統合は既に動作実績あり

2. **明確な分離**
   - picoruby-socket: 低レベルソケットAPI
   - picoruby-net-http: 高レベルHTTP API
   - 依存関係がシンプルで管理しやすい

3. **段階的実装**
   - 各フェーズが独立しており、順次実装可能
   - POSIX版を先行実装し、LwIP版は後から対応

4. **実績のある技術**
   - POSIX sockets: 標準的なネットワークAPI
   - LwIP: 組み込み向けTCP/IPスタックとして実績豊富
   - mbedTLS: セキュアな通信のデファクトスタンダード

#### 技術的課題と対策

| 課題 | 影響度 | 対策 |
|-----|--------|-----|
| **IO継承なし** | 低 | ダックタイピングで十分。CRubyコードの大部分が動作 |
| **LwIPのコールバック** | 中 | ポーリングループで疑似ブロッキング実装 |
| **マイコンのメモリ制約** | 中 | バッファサイズを調整可能に、接続数制限 |
| **TCPServer（LwIP）** | 中 | 最大接続数を3-5に制限、シングルスレッド |
| **select()の制約** | 低 | POSIX版で完全サポート、LwIP版は独自実装 |

#### リスクとマイルストーン

| リスク | 確率 | 対策 |
|-------|------|-----|
| LwIP実装の複雑性 | 中 | Phase 1-5でPOSIX版を完成させてから着手 |
| mbedTLS統合の不具合 | 低 | 既存実装を参考にする |
| メモリ不足 | 中 | バッファサイズを動的調整 |
| パフォーマンス問題 | 低 | プロファイリングして最適化 |

### 成功の指標

1. ✅ POSIX環境でCRuby互換のSocket/Net::HTTPが動作
2. ✅ Raspberry Pi Picoでソケット通信が動作
3. ✅ HTTPSでセキュアな通信が可能
4. ✅ TCPServerで複数クライアントを処理可能
5. ✅ 既存のCRubyコードの80%以上が移植可能

---

## まとめ

### プロジェクトの強み

- ✅ **CRuby互換**: 既存のRubyコードが移植しやすい
- ✅ **デュアルスタック**: POSIX/LwIP両対応で幅広い環境で動作
- ✅ **後方互換**: 既存のpicoruby-netも引き続き使用可能
- ✅ **拡張性**: TCPServer、SSL/TLSなど高度な機能にも対応
- ✅ **実装計画**: 段階的で明確なマイルストーン

### 推奨する開発順序

1. **Phase 1-3**: POSIX版Socket（TCPSocket、UDPSocket、TCPServer）
2. **Phase 4-5**: Net::HTTPとHTTPS対応
3. **Phase 6**: LwIP版Socket（マイコン対応）
4. **Phase 7**: テストとドキュメント

### 期待される成果

- ✅ CRuby互換のネットワークライブラリ
- ✅ HTTPクライアント＆サーバー機能
- ✅ HTTPS対応
- ✅ マイコンでも動作する軽量実装
- ✅ 豊富なドキュメントとサンプルコード

---

## テストとビルド

### テスト方法

PicoRubyには2つのVM実装があり、それぞれ異なるテストコマンドを使用します：

#### mruby/c VM (PicoRuby) のテスト

```bash
# mruby/c実装をテスト
rake test:gems:picoruby[picoruby-socket]
```

- **対象**: `src/mrubyc/socket.c` の実装
- **用途**: 組み込み環境（Raspberry Pi Pico等）向け
- **制約**: 限られた組み込みライブラリ、メタプログラミング機能が限定

#### mruby VM (MicroRuby) のテスト

```bash
# mruby実装をテスト
rake test:gems:microruby[picoruby-socket]
```

- **対象**: `src/mruby/socket.c` の実装
- **用途**: POSIX環境（Linux/macOS）向け
- **機能**: フル機能のRuby互換性

#### VM切り替え時の注意

VM実装を切り替える前には、必ず `rake clean` を実行してください：

```bash
# VM切り替え前にクリーン
rake clean

# その後、テストを実行
rake test:gems:picoruby[picoruby-socket]
# または
rake test:gems:microruby[picoruby-socket]
```

#### テスト実行のベストプラクティス

**重要**: ある程度まとまった作業が完了したら、必ず両方のVM実装に対してテストを実行してください。

```bash
# mruby/c実装のテスト
rake clean && rake test:gems:picoruby[picoruby-socket]

# mruby実装のテスト
rake clean && rake test:gems:microruby[picoruby-socket]
```

**テストを実行すべきタイミング**:
- 新しい機能を実装した後
- バグ修正を行った後
- リファクタリングを行った後
- Phase完了時（コミット前に必須）
- Pull Request作成前

**理由**:
- 両VM実装で動作を確認することで、互換性の問題を早期発見できる
- mruby/c特有の制約による問題を見逃さない
- ビルドキャッシュのクリーンにより、正確なテスト結果が得られる

---

## 開発TIPS

このセクションでは、実際の開発中に発見した重要なヒントとベストプラクティスをまとめます。

### ビルドシステムの仕組み

#### VM別ソースファイルの自動切り替え

PicoRubyのビルドシステムは、VM実装（mruby / mruby/c）に応じて自動的にソースファイルを切り替えます。

**仕組み**:

1. `src/socket.c` がビルド対象として自動的にコンパイルされる
2. この中で `PICORB_VM_MRUBY` または `PICORB_VM_MRUBYC` マクロによって、適切なVMバインディングファイルをincludeする
3. **重要**: `mrbgem.rake` で手動で objfile を追加する必要はない

**ファイル構造例**:
```
mrbgems/picoruby-socket/
├── src/
│   ├── socket.c           # メインファイル（自動コンパイル対象）
│   ├── mruby/
│   │   └── socket.c       # mruby VM用バインディング
│   └── mrubyc/
│       └── socket.c       # mruby/c VM用バインディング
└── ports/
    └── posix/
        └── tcp_socket.c   # POSIX実装（自動コンパイル対象）
```

**src/socket.c の実装パターン**:
```c
#include "../include/socket.h"

#if defined(PICORB_VM_MRUBY)
#include "mruby/socket.c"

#elif defined(PICORB_VM_MRUBYC)
#include "mrubyc/socket.c"

#endif
```

**mrbgem.rake での設定**:
```ruby
MRuby::Gem::Specification.new('picoruby-socket') do |spec|
  spec.license = 'MIT'
  spec.author  = 'PicoRuby developers'

  spec.cc.include_paths << "#{dir}/include"

  # POSIX対応の宣言のみでOK
  spec.posix

  if build.posix?
    spec.cc.defines << 'PICORB_PLATFORM_POSIX'
  end

  # 注意: objfile の手動追加は不要！
  # ビルドシステムが src/ と ports/ 配下を自動で処理
end
```

### 命名規則

#### gem初期化関数の命名規則

mruby VMでのgem初期化関数は、**gemの名前**に基づいて自動的に決定されます。

**規則**:
- gem名が `picoruby-socket` の場合
- 初期化関数: `mrb_picoruby_socket_gem_init`（アンダースコアに変換）
- 終了関数: `mrb_picoruby_socket_gem_final`

**例**:

```c
// ❌ 間違い（mrubyという名前を使用）
void mrb_mruby_socket_gem_init(mrb_state *mrb) { ... }
void mrb_mruby_socket_gem_final(mrb_state *mrb) { ... }

// ✅ 正しい（picorubyという名前を使用）
void mrb_picoruby_socket_gem_init(mrb_state *mrb) { ... }
void mrb_picoruby_socket_gem_final(mrb_state *mrb) { ... }
```

**エラーの症状**:
関数名が正しくない場合、リンカーエラーが発生します：
```
undefined reference to `mrb_picoruby_socket_gem_init'
undefined reference to `mrb_picoruby_socket_gem_final'
```

---

### mruby/c (PicoRuby) の制約

#### 限定された組み込みライブラリ

mruby/c VMは組み込み環境向けに最適化されているため、以下の制約があります：

1. **メタプログラミング機能が限定**
   - `define_method`、`method_missing` 等は使用不可
   - 動的なクラス定義に制約

2. **使用可能なメタプログラミング機能**
   - `picoruby-metaprog` gemの機能は使用可能
   - 必要に応じて `mrbgem.rake` の依存関係に追加：

   ```ruby
   MRuby::Gem::Specification.new('picoruby-socket') do |spec|
     # ... other config ...

     # メタプログラミング機能が必要な場合
     spec.add_dependency 'picoruby-metaprog'
   end
   ```

3. **標準ライブラリの制約**
   - File I/O、正規表現等の一部機能は制限される場合がある
   - ソケット実装ではC言語レベルで実装

4. **メモリ制約**
   - 組み込み環境では数KB〜数百KB程度のメモリ
   - バッファサイズや接続数に制限

#### mruby/c実装の方針

1. **シンプルなAPI**
   - 複雑なメタプログラミングを避ける
   - 直接的なメソッド定義

2. **C言語での実装を優先**
   - 複雑なロジックはC側で実装
   - Ruby側は薄いラッパーに留める

3. **エラーハンドリング**
   - 例外よりも戻り値でのエラー通知を検討
   - メモリ不足等の組み込み特有のエラーに対応

### ビルド設定

#### POSIX環境

```ruby
# mrbgem.rake
if RUBY_PLATFORM =~ /linux|darwin|bsd|unix/i
  spec.cc.defines << 'PICORB_PLATFORM_POSIX'
  spec.objs += Dir.glob("#{dir}/ports/posix/*.c").map { |f|
    objfile(f.pathmap("#{build_dir}/ports/posix/%n"))
  }
end
```

#### LwIP環境（Phase 6で実装）

```ruby
# mrbgem.rake
else
  # LwIP platform
  spec.cc.defines << 'PICORB_PLATFORM_LWIP'
  spec.objs += Dir.glob("#{dir}/src/*.c").map { |f|
    objfile(f.pathmap("#{build_dir}/src/%n"))
  }
end
```

### デバッグ

#### printfデバッグ

```c
#include <stdio.h>

// mruby実装
fprintf(stderr, "[DEBUG] socket fd=%d\n", sock->fd);

// mruby/c実装
// 環境によってはprintfが使えない場合がある
#ifdef DEBUG
  hal_write(1, "[DEBUG] socket connected\n", 26);
#endif
```

#### ログレベル

```c
// socket.h
#define SOCKET_LOG_ERROR   1
#define SOCKET_LOG_WARN    2
#define SOCKET_LOG_INFO    3
#define SOCKET_LOG_DEBUG   4

#ifndef SOCKET_LOG_LEVEL
  #define SOCKET_LOG_LEVEL SOCKET_LOG_ERROR
#endif
```

---

## 参考資料

### 既存実装

- `/home/user/picoruby/mrbgems/picoruby-net/` - 現在のNet実装
- `/home/user/picoruby/mrbgems/picoruby-posix-io/` - IO実装
- `/home/user/picoruby/mrbgems/picoruby-mbedtls/` - TLS/SSL実装

### 外部ライブラリ

- [LwIP](https://savannah.nongnu.org/projects/lwip/) - Lightweight TCP/IP stack
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) - SSL/TLS library
- [CRuby Socket](https://docs.ruby-lang.org/en/master/Socket.html) - Ruby標準ライブラリ
- [CRuby Net::HTTP](https://docs.ruby-lang.org/en/master/Net/HTTP.html) - Ruby標準ライブラリ

---

**設計プラン作成者**: Claude
**最終更新**: 2025-11-12
**ステータス**: ✅ 設計完了、実装準備完了
