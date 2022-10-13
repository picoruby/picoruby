module MRuby
  class Build
    def mrubyc(mrubyc_conf = :default)

      ENV['MRUBYC_BRANCH'] ||= "mrubyc3.1"

      disable_presym

      self.mrbcfile = "#{build_dir}/bin/picorbc"

      cc.defines << "DISABLE_MRUBY"

      gem core: 'picoruby-mrubyc'

      case mrubyc_conf
      when :default
        cc.defines << "MRBC_USE_MATH=1"
        cc.defines << "MRBC_INT64=1"
        cc.defines << "MAX_SYMBOLS_COUNT=#{ENV['MAX_SYMBOLS_COUNT'] || 1000}"
        cc.defines << "MAX_VM_COUNT=#{ENV['MAX_VM_COUNT'] || 255}"
      when :minimum
        # Do noghing
      else
        raise 'Unknown mrubyc_conf'
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
end
