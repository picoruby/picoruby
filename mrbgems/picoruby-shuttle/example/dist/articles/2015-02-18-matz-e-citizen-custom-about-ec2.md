---
layout: default
title: 島根県人がEC2を起動したら最初にやること【Amazon Linux編】
date: 2015-02-18 09:41:28.442935
categories:
---

```
$ sudo yum update
```

```
$ sudo yum install git\
 tree\
 gcc gcc-c++ make\
 libffi libffi-devel\
 zlib zlib-devel\
 openssl openssl-devel\
 httpd-tools\
 libxml2 libxml2-devel\
 libxslt libxslt-devel\
 readline-devel
```

```
$ git clone https://github.com/sstephenson/rbenv.git ~/.rbenv
$ git clone https://github.com/sstephenson/ruby-build.git ~/.rbenv/plugins/ruby-build
$ echo 'export PATH="$HOME/.rbenv/bin:$PATH"' >> ~/.bash_profile
$ echo 'eval "$(rbenv init -)"' >> ~/.bash_profile
$ source .bash_profile
$ rbenv --version
=> rbenv 0.4.0-133-g98953c2
$ rbenv install --list
=> Available versions:
     1.8.6-p383
     ...（略
$ rbenv install 2.2.0
=> Downloading ruby-2.2.0.tar.gz...
   -> http://dqw8nmjcqpjn7.cloudfront.net/7671e394abfb5d262fbcd3b27a71bf78737c7e9347fa21c39e58b0bb9c4840fc
   Installing ruby-2.2.0...
   Installed ruby-2.2.0 to /home/ec2-user/.rbenv/versions/2.2.0
$ rbenv global 2.2.0
$ ruby -v
=> ruby 2.2.0p0 (2014-12-25 revision 49005) [x86_64-linux]
$ gem i bundler
```
