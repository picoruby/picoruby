# TypeProf

An experimental type-level Ruby interpreter for testing and understanding Ruby code.

## Installation

Install via RubyGems.

```sh
$ gem install typeprof
```

### Requirements

TypeProf supports Ruby 3.3 or later.

## Quick start

1. Install VSCode [Ruby TypeProf](https://marketplace.visualstudio.com/items?itemName=mame.ruby-typeprof) extension: `code --install-extension mame.ruby-typeprof`
2. Run `typeprof --init` in your project root to create `typeprof.conf.jsonc` file.
    Other options are available. See [typeprof.conf.jsonc](typeprof.conf.jsonc) for details.

3. Reopen your project in VSCode.

## Development

1. Git clone this repository: `git clone https://github.com/ruby/typeprof.git`
2. Install VSCode [Ruby TypeProf](https://marketplace.visualstudio.com/items?itemName=mame.ruby-typeprof) extension: `code --install-extension mame.ruby-typeprof`
3. Open the repository in VSCode: `code typeprof`

### Testing

```sh
$ bundle install
$ bundle exec rake test
```

## More details

https://speakerdeck.com/mame/good-first-issues-of-typeprof

## LICENSE

See [LICENSE](LICENSE) file.