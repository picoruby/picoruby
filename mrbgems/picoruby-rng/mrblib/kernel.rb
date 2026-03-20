module Kernel
  def rand(max = 0) # steep:ignore
    # @type var max: Integer
    if max == 0
      RNG.random_int
    else
      RNG.random_int % max
    end
  end
end
