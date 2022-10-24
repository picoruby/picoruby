module MRuby
  class Command::Compiler
    attr_accessor :alt_command
  end

  class Command::Archiver
    attr_accessor :alt_command
  end

  class Build
    # Override
    def build_mrbc_exec
      gem :github => 'hasumikin/mruby-bin-picorbc' unless @gems['mruby-bin-picorbc']
      gem :github => 'hasumikin/mruby-pico-compiler' unless @gems['mruby-pico-picocompiler']
      gem core: 'picoruby-mrubyc'
      cc.include_paths.delete_if do |path|
        path.end_with? "hal_no_impl"
      end
      cc.defines.delete_if do |define|
        define.end_with? "hal_no_impl"
      end
      cc.defines << "MRBC_USE_HAL_POSIX"
      hal_obj = objfile("#{gems['picoruby-mrubyc'].build_dir}/src/hal_posix/hal")
      libmruby_objs << hal_obj
      file hal_obj => "#{gems['picoruby-mrubyc'].dir}/repos/mrubyc/src/hal_posix/hal.c" do |f|
        cc.run f.name, f.prerequisites.first
      end
      cc.defines << "MRBC_ALLOC_LIBC"
      cc.defines << "REGEX_USE_ALLOC_LIBC"
      cc.defines << "DISABLE_MRUBY"
      self.mrbcfile = "#{build_dir}/bin/picorbc"
    end

    def picoruby(picoruby_conf = :default)

      ENV['MRUBYC_BRANCH'] ||= "mrubyc3.1"

      disable_presym

      cc.defines << "DISABLE_MRUBY"
      cc.include_paths << "#{gem_clone_dir}/mruby-pico-compiler/include"
      cc.include_paths << "#{build_dir}/mrbgems" # for `#include <picogem_init.c>`
      cc.include_paths << "#{MRUBY_ROOT}/include/hal_no_impl"

      gem core: 'picoruby-mrubyc'

      case picoruby_conf
      when :default
        cc.defines << "MRBC_USE_MATH=1"
        cc.defines << "MRBC_INT64=1"
        cc.defines << "MAX_SYMBOLS_COUNT=#{ENV['MAX_SYMBOLS_COUNT'] || 1000}"
        cc.defines << "MAX_VM_COUNT=#{ENV['MAX_VM_COUNT'] || 255}"
        cc.include_paths << "#{MRUBY_ROOT}/mrbgems/picoruby-mrubyc/repos/mrubyc/src"
      when :minimum
        # Do noghing
      else
        raise "Unknown picoruby_conf: #{picoruby_conf}"
      end

      if ENV["PICORUBY_DEBUG_BUILD"]
        cc.defines << "PICORUBY_DEBUG"
        cc.flags.flatten!
        cc.flags.reject! { |f| %w(-g -O3).include? f }
        cc.flags << "-g3"
        cc.flags << "-O0"
      else
        cc.defines << "NDEBUG"
      end

    end
  end

  module Gem
    class Specification
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
        hal = cc.defines.find { |d| d.start_with?("MRBC_USE_HAL_") }
        if hal
          Dir.glob("#{dir}/src/hal/#{hal.sub("MRBC_USE_HAL_", "").downcase}/*.c").each do |src|
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
end
