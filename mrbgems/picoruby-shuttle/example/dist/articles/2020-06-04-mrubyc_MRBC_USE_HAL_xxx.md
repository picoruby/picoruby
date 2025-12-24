---
layout: default
title: HAL選択方法をちょっと改善した
date: 2020-06-04 13:53:35.116357
categories:
---

HALシリーズ第二段。

[この記事](/hasumin/mrubyc_MRBC_TICK_UNIT)の続きです。

###

いままで、mrubycの各ソースがインクルードするHALのパスは固定で、参照される側をリネームするなりシンボリックリンクでたどるなりして各プラットフォームに対応していました...


...なにを言ってるか自分でもわからないので例示します：

```
/* たとえばsrc/console.hの中にこう書いてあった */
#include "hal/hal.h"
```

そしてたとえばESP32用のアプリケーションを書く場合、 `hal` が `hal_esp32` を指すようにアプリ開発者がsymlinkを貼っていました。

```
% tree src/
.
(...)
├── hal -> hal_esp32
├── hal_esp32
│   ├── hal.c
│   └── hal.h
├── hal_pic24
│   └── hal.h
├── hal_posix
│   ├── hal.c
│   └── hal.h
├── hal_psoc5lp
│   └── hal.h
(...)
```

### わかりやすくなったHALの選択

自分専用の簡単なアプリを書くだけならいままでの方法でも問題ありませんが、複数人での開発だとsymlinkを貼るという手順が抜けやすく、セットアップにつまずいたりしました。
あとからプロジェクトに参加したメンバーが仕組みを把握しづらい方法だったのではないかと思います。

それから、独自のHALを書いたプロジェクトに、upstreamの最新コミットをコンフリクトの心配なくマージできたら捗るよねってことで、以下のような提案をしました：

```
/* src/console.hの中をこう変更しました */
#include "hal_selector.h"
```

ファイル構成：

```
% tree src/
.
(...)
├── hal_esp32
│   ├── hal.c
│   └── hal.h
├── hal_pic24
│   └── hal.h
├── hal_posix
│   ├── hal.c
│   └── hal.h
├── hal_psoc5lp
│   └── hal.h
├── hal_selector.h
├── hal_user_reserved
│   ├── .gitignore     # hal.cとhal.hをignoreします
│   ├── README.md      # （ここにも説明が書いてあります）
│   ├── hal.c          # これはignoreされるので、upstreamと競合しません
│   └── hal.h          # これも。実際にはこれらからアプリ内のHALソースにsymlinkするといいかも
(...)
```

hal_selector.hの中身：

```
#if defined(MRBC_USE_HAL_USER_RESERVED)
#include "hal_user_reserved/hal.h"
#elif defined(MRBC_USE_HAL_PSOC5LP)
#include "hal_psoc5lp/hal.h"
#elif defined(MRBC_USE_HAL_ESP32)
#include "hal_esp32/hal.h"
#elif defined(MRBC_USE_HAL_PIC24)
#include "hal_pic24/hal.h"
#else
#include "hal_posix/hal.h"
#endif /* MRBC_USE_HAL_xxx */
```

これにより、ビルド時のフラグによって使用するHALを選択できます。
たとえばESP32用なら `CFLAGS=-DMRBC_USE_HAL_ESP32 make` 、独自のHALを書いているなら `CFLAGS=-DMRBC_USE_HAL_USER_RESERVED make` とすればよいです。
アプリケーションをビルドするMakefileにこのフラグを書いてもよいし、PSoC CreatorなどのIDEならビルド設定に書けばいいですね。


CFLAGS=-Dxxxx はビルドログに出力されるので、何が起きているのかわかりやすくなりました。
少なくとも、何かをやってるな、ということがログを見る人に伝わる可能性を生み出せました。

気にしない人にとっては非常に細かいことなのですが、こういう工夫でちょっとずつエコシステムを改善しています。


というわけで、mruby/cのためにいろいろなHALを書いてくれる人が増えるといいなーと思っています。

### ところで

今回の提案は、ふたつのプルリクでできています。

[https://github.com/mrubyc/mrubyc/pull/90](https://github.com/mrubyc/mrubyc/pull/90)

[https://github.com/mrubyc/mrubyc/pull/92](https://github.com/mrubyc/mrubyc/pull/92)


最初の提案にはユーザ独自のHALを書きやすくするhal_user_reservedの件を含めず、フラグで既存のHALを選択できることだけにフォーカスしました。

分けたほうが開発チームに説明しやすいだろうと思ったからです。

