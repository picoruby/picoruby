module MRuby
  class Command::Compiler
    attr_accessor :host_command
  end

  class Build
    # Override
    def build_mrbc_exec
      gem core: 'mruby-compiler2' unless @gems['mruby-compiler2']
      gem core: 'mruby-bin-mrbc2' unless @gems['mruby-bin-mrbc2']
      self.mrbcfile = "#{build_dir}/bin/picorbc"
    end

    def create_mrbc_build
      build_mrbc_exec
    end

    def mrubyc_hal_arm
      cc.defines << "MRBC_TICK_UNIT=1"
      cc.defines << "MRBC_TIMESLICE_TICK_COUNT=10"
    end

    def vm_mruby?
      cc.defines.include?("PICORB_VM_MRUBY")
    end

    def vm_mrubyc?
      cc.defines.include?("PICORB_VM_MRUBYC")
    end

    def microruby
      cc.include_paths << "#{gems['mruby-compiler2'].dir}/include"
      cc.include_paths << "#{gems['mruby-compiler2'].dir}/lib/prism/include"
      cc.include_paths << "#{gems['picoruby-mruby'].dir}/lib/mruby/include"
      cc.defines << "PICORB_VM_MRUBY"
      cc.defines << "MRB_USE_TASK_SCHEDULER"
      debug_flag
    end

    def picoruby(alloc_libc: true)
      gem core: "mruby-compiler2" unless @gems['mruby-compiler2']
      gem core: "picoruby-mrubyc" unless @gems['picoruby-mrubyc']
      disable_presym

      # Override by environment variable
      alloc_libc = false if ENV["PICORUBY_NO_LIBC_ALLOC"]

      cc.defines << "PICORB_VM_MRUBYC"
      cc.defines << (alloc_libc ? "MRBC_ALLOC_LIBC" : "MRBC_USE_ALLOC_PROF")
      cc.defines << "DISABLE_MRUBY"
      cc.defines << "MRBC_INT64" if cc.defines.include?("PICORUBY_INT64")
      %w(MRBC_USE_MATH=1 MAX_SYMBOLS_COUNT=1000 MAX_VM_COUNT=255 MAX_REGS_SIZE=255).each do |define|
        key, _value = define.split("=")
        cc.defines << define if cc.defines.none? { _1.start_with? key }
      end

      timestamp = Time.at(`git log -1 --format=%ct`.to_i).utc.strftime('%Y-%m-%dT%H:%M:%SZ')
      branch = `git branch --show-current`.strip
      commit_hash = `git log -1 --format=%h`.strip

      File.write(
        "#{MRUBY_ROOT}/src/version.c",
        File.read("#{MRUBY_ROOT}/src/version.c.in")
            .gsub('@PICORUBY_COMMIT_TIMESTAMP@', timestamp)
            .gsub('@PICORUBY_COMMIT_BRANCH@', branch)
            .gsub('@PICORUBY_COMMIT_HASH@', commit_hash)
      )

      cc.include_paths << "#{gems['picoruby-mrubyc'].dir}/include"
      cc.include_paths << "#{gems['mruby-compiler2'].dir}/include"
      cc.include_paths << "#{gems['mruby-compiler2'].dir}/lib/prism/include"
      cc.include_paths << "#{MRUBY_ROOT}/include/picoruby"
      cc.include_paths << "#{build_dir}/mrbgems" # for `#include <picogem_init.c>`
      mrubyc_dir = "#{gems['picoruby-mrubyc'].dir}/lib/mrubyc"
      cc.include_paths << mrubyc_dir + "/src"
      if cc.build.posix?
        cc.include_paths << mrubyc_dir + "/hal/posix"
      else
        cc.include_paths << gems['picoruby-machine'].dir + '/include'
      end

      debug_flag
    end

    def posix?
      self.name == 'host' || cc.defines.include?("MRBC_USE_HAL_POSIX") || cc.defines.include?("PICORB_PLATFORM_POSIX")
    end

    private def debug_flag
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
          if build.cc.defines.include?("PICORB_VM_MRUBYC") && name.start_with?("picoruby-")
            rbfiles.clear
          end
          generate_gem_init("#{build_dir}/gem_init.c")
        end
      end

      def posix
        return unless cc.build.posix?
        ["posix", "common"].each do |subdir|
          Dir.glob("#{dir}/ports/#{subdir}/**/*.c").each do |src|
            obj = objfile(src.pathmap("#{build_dir}/ports/#{subdir}/%n"))
            build.libmruby_objs << obj
            file obj => src do |f|
              cc.run f.name, f.prerequisites.first
            end
          end
        end
      end
    end
  end

end
