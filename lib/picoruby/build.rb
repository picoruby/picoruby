module MRuby
  class Command::Compiler
    attr_accessor :host_command
  end

  class Build
    # Override
    def build_mrbc_exec
      gem github: 'picoruby/mruby-compiler2' unless @gems['mruby-compiler2']
      gem github: 'picoruby/mruby-bin-mrbc2' unless @gems['mruby-bin-mrbc2']
      gem core: 'picoruby-mrubyc'
      cc.include_paths.delete_if do |path|
        path.end_with? "hal_no_impl"
      end
      cc.defines.delete_if do |define|
        define.end_with? "hal_no_impl"
      end
      cc.defines << "MRBC_USE_HAL_POSIX"
      cc.defines << "MRBC_ALLOC_LIBC"
      cc.defines << "DISABLE_MRUBY"
      self.mrbcfile = "#{build_dir}/bin/picorbc"
    end

    def mrubyc_lib_dir
      # Also used in picoruby-mrubyc
      ENV['MRUBYC_LIB_DIR'] ||= "#{gems['picoruby-mrubyc'].dir}/lib"
    end

    def mrubyc_dir
      mrubyc_lib_dir + "/mrubyc"
    end


    def mrubyc_src_dir
      mrubyc_dir + "/src"
    end

    def mrubyc_hal_arm
      cc.defines << "MRBC_TICK_UNIT=1"
      cc.defines << "MRBC_TIMESLICE_TICK_COUNT=10"
    end

    def porting(dir)
      d = cc.defines.find { _1.start_with?("PICORUBY_PLATFORM=") }
      return unless d
      platform = d.split("=")[1]
      if platform.nil?
        raise "PICORUBY_PLATFORM is not set"
      end
      Dir.glob("#{dir}/ports/#{platform}/**/*.c").each do |src|
        obj = objfile(src.pathmap("#{build_dir}/ports/posix/%n"))
        libmruby_objs << obj
        file obj => src do |f|
          cc.run f.name, f.prerequisites.first
        end
      end
    end

    def picoruby

      disable_presym

      cc.defines << "DISABLE_MRUBY"
      cc.include_paths << "#{build_dir}/mrbgems" # for `#include <picogem_init.c>`

      gem github: "picoruby/mruby-compiler2"
      cc.include_paths << cc.build.gems['mruby-compiler2'].dir + '/include'
      gem core: 'picoruby-mrubyc'

      %w(MRBC_USE_MATH=1 MRBC_INT64=1 MAX_SYMBOLS_COUNT=1000 MAX_VM_COUNT=255 MAX_REGS_SIZE=255).each do |define|
        key, _value = define.split("=")
        cc.defines << define if cc.defines.none? { _1.start_with? key }
      end
      cc.include_paths << mrubyc_src_dir
      if cc.defines.include?("PICORUBY_PLATFORM=posix")
        cc.include_paths << mrubyc_dir + "/hal/posix"
      else
        cc.include_paths << "#{MRUBY_ROOT}/include/hal_no_impl"
      end
      cc.include_paths << gems['mruby-compiler2'].dir + "/lib/prism/include"

      if /darwin/ =~ RUBY_PLATFORM && system("which brew > /dev/null 2>&1") # for macOS && homebrew
        linker.library_paths << File.join(`brew --prefix`.chomp, "/lib")

        openssl_path = `brew --prefix openssl`.chomp
        if openssl_path != "" && openssl_path != nil
          cc.include_paths << File.join(openssl_path, "include")
        end
      end

      cc.flags.flatten!
      cc.flags.reject! { |f| %w(-g -g1 -g2 -g3 -O0 -O1 -O2 -O3).include? f }
      if ENV["PICORUBY_DEBUG"]
        cc.defines << "PICORUBY_DEBUG=1"
        cc.flags << "-O0"
        cc.flags << "-g3"
        cc.flags << "-fno-inline"
      else
        cc.defines << "NDEBUG=1"
        cc.flags << "-Os"
        cc.flags << "-finline-functions"
        cc.flags << "-ffunction-sections"
        cc.flags << "-fdata-sections"
        cc.flags << "-fomit-frame-pointer"
        # cc.flags << "-flto" # Build fails with -flto
        unless cc.command == "clang"
          cc.flags << "-s"
          cc.flags << "-Wl,--gc-sections"
        end
      end

    end
  end

  module Gem
    class Specification
      attr_accessor :require_name

      def define_gem_init_builder
        file "#{build_dir}/gem_init.c" => [build.mrbcfile, __FILE__] + [rbfiles].flatten do |t|
          mkdir_p build_dir
          if build_dir.include?("mrbgems/picoruby-")
            rbfiles.clear
          end
          generate_gem_init("#{build_dir}/gem_init.c")
        end
      end

      def hal_obj
        Dir.glob("#{dir}/src/hal/*.c").each do |src|
          obj = "#{build_dir}/src/#{objfile(File.basename(src, ".c"))}"
          file obj => src do |t|
            cc.run(t.name, t.prerequisites[0])
          end
          objs << obj
        end
      end
    end
  end
end
