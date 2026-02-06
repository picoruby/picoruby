require "fileutils"

R2P2_GEM_DIR = "#{MRUBY_ROOT}/mrbgems/picoruby-r2p2"
load "#{R2P2_GEM_DIR}/r2p2_config.rb"

ENV["PICO_SDK_PATH"] ||= "#{R2P2_GEM_DIR}/lib/pico-sdk"
ENV["PICO_EXTRAS_PATH"] ||= "#{R2P2_GEM_DIR}/lib/pico-extras"

def r2p2_mruby_config(vm, board)
  "#{MRUBY_ROOT}/build_config/r2p2-#{vm}-#{board}.rb"
end

def r2p2_def_board(board)
  case board
  when 'pico2_w'
    '-D PICO_PLATFORM=rp2350 -D PICO_BOARD=pico2_w -D USE_WIFI=1'
  when 'pico2'
    '-D PICO_PLATFORM=rp2350 -D PICO_BOARD=pico2'
  when 'pico_w'
    '-D PICO_PLATFORM=rp2040 -D PICO_BOARD=pico_w -D USE_WIFI=1'
  else
    '-D PICO_PLATFORM=rp2040 -D PICO_BOARD=pico'
  end
end

def r2p2_def_build_type(mode)
  case mode
  when 'debug'
    "-D CMAKE_BUILD_TYPE=Debug -D PICORUBY_DEBUG=1"
  else
    "-D CMAKE_BUILD_TYPE=Release -D NDEBUG=1"
  end
end

def r2p2_def_r2p2_name(vm, board)
  version = File.read("#{MRUBY_ROOT}/include/version.h").match(/#define PICORUBY_VERSION "(.+?)"/)[1]
  "-D R2P2_NAME=R2P2-#{vm.upcase} -D R2P2_BOARD_NAME=#{board.upcase} -D R2P2_VERSION=#{version}"
end

def r2p2_def_msc(mode)
  '-D PICORUBY_MSC_FLASH=1'
end

def r2p2_def_picorb_vm(vm)
  vm == 'picoruby' ? '-D PICORB_VM_MRUBYC=1' : '-D PICORB_VM_MRUBY=1'
end

def r2p2_build_dir(vm, board, mode)
  "#{MRUBY_ROOT}/build/r2p2/#{vm}/#{board}/#{mode}"
end

namespace :r2p2 do
  desc "Initialize git submodules for R2P2"
  task :setup do
    FileUtils.cd MRUBY_ROOT do
      sh "git submodule update --init mrbgems/picoruby-r2p2/lib/pico-sdk"
      sh "git submodule update --init mrbgems/picoruby-r2p2/lib/pico-extras"
      sh "git submodule update --init mrbgems/picoruby-r2p2/lib/rp2040js"
    end
  end

  task :check_pico_sdk => :check_pico_sdk_path do
    FileUtils.cd ENV['PICO_SDK_PATH'] do
      if `git describe --tags --exact-match`.chomp != PICO_SDK_TAG
        raise <<~MSG
          pico-sdk #{PICO_SDK_TAG} is not checked out!\n
          Tips for dealing with:\n
          cd #{ENV['PICO_SDK_PATH']} && \\
            git fetch origin --tags && \\
            git checkout #{PICO_SDK_TAG} && \\
            git submodule update --recursive\n
        MSG
      end
    end
    FileUtils.cd ENV['PICO_EXTRAS_PATH'] do
      if `git describe --tags --exact-match`.chomp != PICO_EXTRAS_TAG
        raise <<~MSG
          pico-extras #{PICO_EXTRAS_TAG} is not checked out!\n
          Tips for dealing with:\n
          cd #{ENV['PICO_EXTRAS_PATH']} && \\
            git fetch origin --tags && \\
            git checkout #{PICO_EXTRAS_TAG} && \\
            git submodule update --recursive\n
        MSG
      end
    end
  end

  task :check_pico_sdk_path do
    %w(PICO_SDK_PATH PICO_EXTRAS_PATH).each do |env|
      unless ENV[env]
        raise <<~MSG
          Environment variable `#{env}` does not exist!
        MSG
      end
    end
    # Check that pico-sdk submodules (e.g. tinyusb) are checked out
    pico_sdk_path = ENV['PICO_SDK_PATH']
    unless File.exist?("#{pico_sdk_path}/lib/tinyusb/src/tusb.h")
      raise <<~MSG
        pico-sdk submodules are not checked out!
        Run: cd #{pico_sdk_path} && git submodule update --init --recursive
      MSG
    end
  end

  %w[picoruby microruby].each do |vm|
    namespace vm do
      %w[pico pico_w pico2 pico2_w].each do |board|
        config_file = "#{MRUBY_ROOT}/build_config/r2p2-#{vm}-#{board}.rb"
        next unless File.exist?(config_file)

        namespace board do
          %w[debug prod].each do |mode|
            desc "Build R2P2 #{vm} for #{board} (#{mode})"
            task mode => "r2p2:check_pico_sdk" do
              dir = r2p2_build_dir(vm, board, mode)
              FileUtils.mkdir_p dir
              config = r2p2_mruby_config(vm, board)
              mruby_build_path = "#{MRUBY_ROOT}/build/r2p2-#{vm}-#{board}"
              FileUtils.cd MRUBY_ROOT do
                sh "MRUBY_CONFIG=#{config} #{mode=='debug' ? 'PICORUBY_DEBUG=1' : ''} rake"
              end
              defs = <<~DEFS
                -D PICORUBY_ROOT=#{MRUBY_ROOT} \
                -D R2P2_GEM_DIR=#{R2P2_GEM_DIR} \
                -D EXTRA_LIBRARY_PATH=#{mruby_build_path}/lib \
                -D EXTRA_INCLUDE_DIR=#{mruby_build_path}/include \
                -D PICO_CYW43_SUPPORTED=1 \
                -D MRUBY_CONFIG=r2p2-#{vm}-#{board} \
                #{r2p2_def_picorb_vm(vm)} \
                #{r2p2_def_r2p2_name(vm, board)} \
                #{r2p2_def_board(board)} \
                #{r2p2_def_build_type(mode)} \
                #{r2p2_def_msc(mode)}
              DEFS
              sh "cmake -S #{R2P2_GEM_DIR}/cmake -B #{dir} #{defs}"
              sh "cmake --build #{dir}"
            end
          end

          desc "Clean R2P2 #{vm} for #{board} (both debug and prod)"
          task :clean do
            FileUtils.cd MRUBY_ROOT do
              config = r2p2_mruby_config(vm, board)
              if File.exist?(config)
                sh "MRUBY_CONFIG=#{config} rake clean"
              end
            end
            %w[debug prod].each do |mode|
              dir = r2p2_build_dir(vm, board, mode)
              FileUtils.rm_f(Dir["#{dir}/R2P2*.*"]) if Dir.exist? dir
            end
          end
        end
      end
    end
  end
end
