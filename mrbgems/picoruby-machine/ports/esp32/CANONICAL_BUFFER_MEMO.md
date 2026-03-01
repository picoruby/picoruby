# ESP32向け Canonical Buffer 対応メモ

## RP2040側でやったこと

### 1. `io_echo_eq` の反転バグ修正
- `ports/rp2040/io-console.c` と `ports/esp32/io-console.c` の両方を修正済み
- 修正前: `io_echo_eq(true)` で `echo_mode = false` になっていた（POSIX と逆）
- 修正後: `echo_mode = flag` に統一

### 2. `hal.h` に定数追加
```c
#define HAL_GETCHAR_NODATA  (-1)
#define HAL_GETCHAR_EOF     (-2)
```

### 3. RP2040の `hal_getchar()` に canonical buffer を実装
- cooked mode（`io_raw_q() == false`）のとき、HAL層でエコー・バックスペース編集・Ctrl-D(EOF)を処理
- raw mode のときは RingBuffer から直接バイトを返す

### 4. `IO#gets` / `IO#read` の簡略化（mruby/mrubyc共通）
- エコー出力（`hal_write`）、バックスペース処理（`"\b \b"`）、ESCスキップのロジックを削除
- `HAL_GETCHAR_EOF`（-2）のハンドリングを追加
- `IO#read` を引数なしで呼べるように変更（EOF まで読む）

## ESP32で予想される影響

### おそらく問題ないこと
- `io_echo_eq` の修正は適用済みなので、echo フラグの意味が正しくなった
- ESP-IDF の VFS/UART ドライバが canonical 処理（エコー、行編集）を担当しているなら、IO メソッドからエコー/BS ロジックを削除しても影響なし

### 確認が必要なこと
1. EOF の区別: ESP32 の `hal_getchar()` は `fgetc(stdin)` を使っており、`EOF`（-1）を返す。これは `HAL_GETCHAR_NODATA`（-1）と同じ値なので、`IO#read`（引数なし）が EOF を検出できず永久ブロックする可能性がある
2. エコー: ESP-IDF の VFS が UART の echo を処理しているか確認。していなければ `IO#gets` で入力が見えなくなる
3. バックスペース: 同様に VFS 側で処理されているか確認

### ESP32側で対応が必要になりそうなこと
- `hal_getchar()` で `fgetc()` の戻り値が `EOF` のとき `HAL_GETCHAR_EOF`（-2）を返すようにする
- VFS が canonical 処理をしていない場合は、RP2040 と同様の canonical buffer を実装するか、VFS の設定で有効にする
