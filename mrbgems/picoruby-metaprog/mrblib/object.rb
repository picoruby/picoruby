class Object
  def instance_eval(&block)
    self_save = block._get_self
    block._set_self(self)
    result = block.call(self)
    block._set_self(self_save)
    result
  end
end
