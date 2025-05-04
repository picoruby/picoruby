if RUBY_ENGINE == "mruby"
  class Time
    def self.now(in: nil)
      ts = Machine.get_hwclock
      tv_sec = ts[0]
      tv_nsec = ts[1].to_f / 1_000_000_000
      Time.at(tv_sec + tv_nsec)
    end
  end
end
