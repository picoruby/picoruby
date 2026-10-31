# Support the generated C calls into.
module PicoRubyAOT
  # A call with keywords. mruby has no C API that passes keywords, so the
  # generated C hands the receiver, the method, the positional arguments,
  # the keyword Hash and the block (or nil) here, and the VM makes the
  # call.
  def self.send_kw(recv, mid, args, kw, blk)
    recv.__send__(mid, *args, **kw, &blk)
  end
end
