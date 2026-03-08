# Hybrid implementation: C helper class + Ruby wrappers
# C implementation: PackHelper class in src/mrubyc/pack.c
# Ruby wrappers: Array#pack and String#unpack

class Array
  def pack(format, buffer: nil)
    PackHelper.pack_array(self, format)
  end
end

class String
  def unpack(format, offset: 0)
    PackHelper.unpack_string(self, format)
  end
end
