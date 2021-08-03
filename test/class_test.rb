class ClassTest < PicoRubyTest
  desc "re-define Array#collect"
  assert_equal(<<~RUBY, "[\"1\", \"2\"]")
    def collect
      i = 0
      ary = []
      while i < length
        ary[i] = yield self[i]
        i += 1
      end
      return ary
    end
    p [1, 2].collect {|e| e.to_s }
  RUBY
end
