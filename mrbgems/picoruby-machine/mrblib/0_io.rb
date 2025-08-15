class IO
  if RUBY_ENGINE == "mruby"
    def self.open(*args)
      self.new # TODO
    end
  end
end
