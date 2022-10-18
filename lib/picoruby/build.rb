module MRuby
  class Build
    def picoruby(picoruby_conf = :default)

      ENV['MRUBYC_BRANCH'] ||= "mrubyc3.1"

      disable_presym

      self.mrbcfile = "#{build_dir}/bin/picorbc"

      cc.defines << "DISABLE_MRUBY"
      cc.include_paths << "#{build_dir}/mruby-pico-compiler/include"
      cc.include_paths << "#{build_dir}/mrbgems" # for `#include <picogem_init.c>`

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

      cc.defines << ENV.keys.find { |env|
        env.start_with? "MRBC_USE_HAL"
      }.then { |hal|
        if hal.nil?
          "MRBC_USE_HAL_POSIX"
        elsif hal == "MRBC_USE_HAL"
          "#{hal}=#{ENV[hal]}"
        elsif hal.start_with?("MRBC_USE_HAL_")
          hal
        else
          raise "Invalid MRBC_USE_HAL_ definition!"
        end
      }

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
    end
  end
end
