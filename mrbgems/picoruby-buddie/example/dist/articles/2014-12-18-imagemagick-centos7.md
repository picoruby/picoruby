---
layout: default
title: ImageMagickのインストール時に注意すること【CentOS7編】
date: 2014-12-18 15:05:59.364260
categories:
---

CentOSの場合はソースコードからインストールしてください。
yumではいる古いやつは品質のわるいコンバートしかできなかったりします。

### 依存パッケージをインストール

インストール前には

```
sudo yum install openjpeg-devel libjpeg-turbo-devel libtiff-devel libgeotiff-devel libpng-devel giflib-devel libexif-devel libexif libwmf-devel libwmf libtool-ltdl-devel libtool-ltdl lcms-devel
```

これら依存パッケージ（手元のCentOS7に入ってなかったのをテキトウに詰め込みました）を入れてから……

### ./configure



```
./configure
```

すると、自動的に使えるライブラリが読み込まれます。ターミナル出力の終わりの方の、

```
JPEG v1           --with-jpeg=yes   yes
PNG               --with-png=yes    yes
TIFF              --with-tiff=yes   yes
```

このへんのにyesがつけばだいたいOKです。
案件によってはJPEG-2000あたりも要るかもです（営業さんそんな案件とってこないでください！ｍｍ）。

それから

```
make
sudo make install
```

でよいです。

もしも gem 'carrierwave' が `mini_magick_processing_error` とか、gem 'minimagick' が `MiniMagick::Invalid` とか吐いたら、jpeg対応が外れているかも、と本記事を思い出してください。

### ちなみにソースからの再インストールは

```
sudo make uninstall
make clean
```

その後、上のほうに書いた依存パッケージを入れて、再度 `./configure` からやり直せばよいです。

### 追記。ImageMagicインストール後にJPEG対応を確認する方法

```
identify -list format | grep JPG
```

と打って、

```
JPG* rw-   Joint Photographic Experts Group JFIF format (62)
```

と出ればおｋ
