module MRuby
  class Command::Compiler
    attr_accessor :host_command
  end

  class Build
    # Override
    def build_mrbc_exec
      gem github: 'picoruby/mruby-compiler2' unless @gems['mruby-compiler2']
      gem github: 'picoruby/mruby-bin-mrbc2' unless @gems['mruby-bin-mrbc2']
      self.mrbcfile = "#{build_dir}/bin/picorbc"
    end

    def create_mrbc_build
      build_mrbc_exec
    end

    def mrubyc_lib_dir
      # Also used in picoruby-mrubyc
      "#{gems['picoruby-mrubyc'].dir}/lib"
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

    def microruby
      cc.include_paths << "#{gems['picoruby-mruby'].dir}/lib/mruby/include"
      cc.flags << "-O0" if ENV['PICORUBY_DEBUG']
      cc.defines << "PICORB_VM_MRUBY"
    end

    def picoruby(alloc_libc: true)

      disable_presym

      if alloc_libc
        cc.defines << "MRBC_ALLOC_LIBC"
      else
        cc.defines << "MRBC_USE_ALLOC_PROF"
      end
      cc.defines << "PICORB_VM_MRUBYC"
      cc.defines << "DISABLE_MRUBY"
      cc.include_paths << "#{MRUBY_ROOT}/include/picoruby"
      cc.include_paths << "#{build_dir}/mrbgems" # for `#include <picogem_init.c>`

      gem github: "picoruby/mruby-compiler2"
      cc.include_paths << cc.build.gems['mruby-compiler2'].dir + '/include'
      gem core: 'picoruby-mrubyc'

      if cc.defines.include?("PICORUBY_INT64")
        cc.defines << "MRBC_INT64"
      end
      %w(MRBC_USE_MATH=1 MAX_SYMBOLS_COUNT=1000 MAX_VM_COUNT=255 MAX_REGS_SIZE=255).each do |define|
        key, _value = define.split("=")
        cc.defines << define if cc.defines.none? { _1.start_with? key }
      end
      cc.include_paths << mrubyc_src_dir
      if cc.defines.include?("PICORUBY_PLATFORM=posix")
        cc.include_paths << mrubyc_dir + "/hal/posix"
      else
        # assuming microcontroller platform
        cc.include_paths << gems['picoruby-machine'].dir + '/include'
      end
      cc.include_paths << gems['mruby-compiler2'].dir + "/lib/prism/include"

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
          linker.flags << "-Wl,--gc-sections"
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
    end
  end

end
