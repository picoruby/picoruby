class Array
  def each
    i = 0
    while i < length
      yield self[i]
      i += 1
    end
   return self
  end
end

[0,3].each do |a|
  p a
end
