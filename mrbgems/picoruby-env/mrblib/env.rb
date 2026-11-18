class ENVClass
  include Enumerable

  def each
    h = _hash
    keys = h.keys
    keys_size = keys.size
    i = 0
    while i < keys_size
      key = keys[i]
      yield(key, h[key])
      i += 1
    end
    self
  end
  alias each_pair each

  def each_key
    keys = _hash.keys
    keys_size = keys.size
    i = 0
    while i < keys_size
      yield(keys[i])
      i += 1
    end
    self
  end

  def each_value
    values = _hash.values
    values_size = values.size
    i = 0
    while i < values_size
      yield(values[i])
      i += 1
    end
    self
  end

  # A Hash of the variables; with a block, of the pairs it answers.
  def to_h
    h = _hash
    return h.dup unless block_given?
    result = {}
    keys = h.keys
    keys_size = keys.size
    i = 0
    while i < keys_size
      key = keys[i]
      pair = yield(key, h[key])
      result[pair[0]] = pair[1]
      i += 1
    end
    result
  end

  def to_hash
    _hash.dup
  end

  def keys
    _hash.keys
  end

  def values
    _hash.values
  end

  def key?(name)
    _hash.has_key?(name)
  end
  alias has_key? key?
  alias include? key?
  alias member? key?

  def value?(value)
    _hash.has_value?(value)
  end
  alias has_value? value?

  def key(value)
    _hash.key(value)
  end

  def size
    _hash.size
  end
  alias length size

  def empty?
    _hash.empty?
  end

  # Hash#fetch raises the error for a missing name, KeyError where the VM
  # has one.
  def fetch(name, *default)
    h = _hash
    return h[name] if h.has_key?(name)
    return yield(name) if block_given?
    return default[0] unless default.empty?
    h.fetch(name)
  end

  def values_at(*names)
    h = _hash
    result = []
    names_size = names.size
    i = 0
    while i < names_size
      result << h[names[i]]
      i += 1
    end
    result
  end

  def slice(*names)
    h = _hash
    result = {}
    names_size = names.size
    i = 0
    while i < names_size
      name = names[i]
      result[name] = h[name] if h.has_key?(name)
      i += 1
    end
    result
  end

  def assoc(name)
    h = _hash
    h.has_key?(name) ? [name, h[name]] : nil
  end

  # Sets each variable of each hash; with a block, a name already set takes
  # what the block answers for (name, old value, new value).
  def update(*hashes)
    hashes_size = hashes.size
    i = 0
    while i < hashes_size
      hash = hashes[i]
      keys = hash.keys
      keys_size = keys.size
      j = 0
      while j < keys_size
        key = keys[j]
        value = hash[key]
        if block_given? && key?(key)
          value = yield(key, self[key], value)
        end
        self[key] = value
        j += 1
      end
      i += 1
    end
    self
  end
  alias merge! update

  def clear
    keys = _hash.keys
    keys_size = keys.size
    i = 0
    while i < keys_size
      delete(keys[i])
      i += 1
    end
    self
  end

  # A Hash of the variables the block answers true for.
  def select
    h = _hash
    result = {}
    keys = h.keys
    keys_size = keys.size
    i = 0
    while i < keys_size
      key = keys[i]
      value = h[key]
      result[key] = value if yield(key, value)
      i += 1
    end
    result
  end
  alias filter select

  # A Hash of the variables the block answers false for.
  def reject
    h = _hash
    result = {}
    keys = h.keys
    keys_size = keys.size
    i = 0
    while i < keys_size
      key = keys[i]
      value = h[key]
      result[key] = value unless yield(key, value)
      i += 1
    end
    result
  end

  def inspect
    _hash.inspect
  end

  def to_s
    "ENV"
  end
end

ENV = ENVClass.new

ENV_DEFAULT_WIFI_CONFIG_PATH = "/etc/network/wifi.yml"
ENV_DEFAULT_HOME = "/home"
