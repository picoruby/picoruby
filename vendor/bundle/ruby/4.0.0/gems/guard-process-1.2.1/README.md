# Guard::Process [![Build Status](https://secure.travis-ci.org/guard/guard-process.png)](http://travis-ci.org/guard/guard-process)
Guard to run processes and commands.

This gem requires Ruby 1.9.3, 2.0.0 or JRuby in 1.9 mode.

# Usage
Please read the [Guard documentation](https://github.com/guard/guard#readme) to learn how to use Guard.

There is also an exellent screencast available on [Railscasts](http://railscasts.com/episodes/264-guard)

Additionally there is a great introductory article on [Intridea Blog](http://intridea.com/2011/8/25/hire-a-guard-for-your-project) mentioning Guard::Process.

# Guardfile
You can add as many process Guards as you want, an example Guardfile:

``` ruby
  guard 'process', :name => 'EndlessRunner', :command => 'rails runner Something::ThatGoesOnAndOn' do
    watch('Gemfile.lock')
  end

  guard 'process', :name => 'AnotherRunner', :command => ['rails', 'runner', 'AnotherRunner'] do
    watch('Gemfile.lock')
  end
```

# Options
The following options are available:

- name
  The display name of your process in Guard, this is shown when Guard::Process is starting and stopping your process
- command
  The command to run as you would run it from the command line. NOTE: You cannot string commands together like you would on the shell with ; or && because of the way guard-process launches its processes, if you need to combine several shell commands you should write a shell script and call from your command.
- dir
  The directory in which you want to run the command, if none is given then Ruby's current working directory is used
- stop_signal
  The signal that Guard::Process sends to terminate the process when it needs to stop/restart it. This defaults to 'TERM' (in some cases you may need KILL).
- dont_stop
  When a reload is triggered this will not stop the process, instead it will wait for the current process run to finish before starting a new one. This is useful for having guard-process execute commands that don't run constantly that you don't want to interrupt as they run.
- env
  A Hash with environmental variables to set for the context where your command runs, like so:
``` ruby
  {"SSL_CERTS_DIR" => "/etc/ssl/certs", "JAVA_HOME" => "/usr/local/java"}
```

# TODO

Clean up the test cases

# Changes

## 1.1.0

- Updated for Guard 2.x

## 1.0.6

- Allow passing in command as an Array as well as a String

## 1.0.5

- Adds the dont_stop option

## 1.0.4

- Adds the dir option

# Development
- Source hosted on [GitHub](https://github.com)
- Please report issues and feature requests using [GitHub issues](https://github.com/guard/guard-process/issues)

Pull requests are welcome, please make sure your patches are well tested before contributing. When sending a pull request:
- Use a topic branch for your change
- Add tests for your changes using MiniTest::Unit
- Do not change the version number in the gemspec

# License
Guard::Process is released under the MIT license.

# Author
[Mark Kremer](https://github.com/mkremer)
