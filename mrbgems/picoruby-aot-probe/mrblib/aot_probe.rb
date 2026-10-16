# Methods whose parameters and results Spinel proves Integer or Float,
# so picoruby-aot compiles them with unboxed slots. The tests hand them
# values past the 31-bit fixnum of a word-boxing build and past mrb_int
# of an MRB_INT32 build, where the irep answers with a Bignum. The twin
# on the 32-bit host build (test:gems:aot_twin32) is where that shows.
module AOTProbe
  def self.add(a, b)
    a + b
  end

  def self.sub(a, b)
    a - b
  end

  def self.mul(a, b)
    a * b
  end

  def self.div(a, b)
    a / b
  end

  def self.mod(a, b)
    a % b
  end

  def self.shl(a, n)
    a << n
  end

  def self.neg(a)
    -a
  end

  def self.same(a)
    a
  end

  def self.to_int(f)
    f.to_i
  end

  def self.to_float(a)
    a.to_f
  end

  def self.later?(a, b)
    a < b
  end

  # A microsecond delta as a clock keeps it: never negative.
  def self.elapsed(now_us, last_us)
    elapsed = now_us - last_us
    elapsed = 0 if elapsed < 0
    elapsed
  end

  # Keyword parameters: a required one takes its proved kind, an optional
  # one evaluates its default in method scope.
  def self.scale(value, by:, offset: 0)
    value * by + offset
  end

  # A call with keywords to a compiled def of this gem (a direct call) and
  # to a Proc (the VM path).
  def self.scaled_twice(value, by)
    scale(scale(value, by: by), by: by, offset: 1)
  end

  def self.call_with_keywords(callee, value)
    callee.call(value, by: 3)
  end

  # Blocks a compiled def receives: a `&blk` parameter called in a loop,
  # `yield`, `block_given?`, and `&blk` forwarded to a compiled def (a
  # direct call), to a core method (the VM) and with keywords.
  def self.each_index_of(a, &blk)
    i = 0
    while i < a.size
      blk.call(a[i], i)
      i += 1
    end
    a.size
  end

  def self.twice(x)
    r = yield(x)
    r + yield(x)
  end

  def self.maybe(x)
    if block_given?
      yield x
    else
      x
    end
  end

  def self.forward(a, &blk)
    each_index_of(a, &blk)
  end

  def self.forward_to_vm(a, &blk)
    a.map(&blk)
  end

  def self.forward_with_keywords(callee, value, &blk)
    callee.call(value, by: 3, &blk)
  end

  def self.call_or_value(x, &blk)
    return x unless blk
    blk.call(x)
  end

  def self.without_block(x)
    maybe(x)
  end

  # Calls the classifier resolves by the receiver's type, not by name:
  # `join` on an Array is Array#join, not Task#join (joined compiles);
  # the body of a define_method block does not run when the irep loads,
  # so its `==` does not keep Tagged#== in the irep; and the class-body
  # call `Registry.define` names the singleton method only, so
  # Builder#define compiles while Registry.define stays in the irep.
  def self.joined(a)
    a.first(2).join(",")
  end

  class Tagged
    def ==(other)
      other.is_a?(Tagged)
    end

    %w[one two].each do |name|
      define_method(name) { |x| x == 1 } # steep:ignore
    end
  end

  module Registry
    def self.define(x)
      x + 1
    end
  end

  class Builder
    def define(x)
      x * 2
    end
  end

  DEFINED = Registry.define(1)

  # A timestamp held in an instance variable across calls.
  class Clock
    def initialize
      @last_us = 0
    end

    attr_accessor :last_us

    def observe(now_us)
      delta = now_us - @last_us
      @last_us = now_us
      delta
    end
  end
end
