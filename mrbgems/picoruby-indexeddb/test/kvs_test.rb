class IndexedDBKVSTest < Picotest::Test
  def setup
    # Clear any existing data between tests
    IndexedDB::InMemoryDatabase.stores = {}
    IndexedDB::InMemoryDatabase.meta = {}
  end

  def test_open_creates_kvs
    kvs = IndexedDB::KVS.open('test_db')
    assert_equal('kv', kvs.name)
  end

  def test_open_with_custom_store_name
    kvs = IndexedDB::KVS.open('test_db', store: 'custom_store')
    assert_equal('custom_store', kvs.name)
  end

  def test_set_and_get_value
    kvs = IndexedDB::KVS.open('test_db')
    kvs['key1'] = 'value1'
    assert_equal('value1', kvs['key1'])
  end

  def test_get_nonexistent_key_returns_nil
    kvs = IndexedDB::KVS.open('test_db')
    assert_nil(kvs['nonexistent'])
  end

  def test_delete_key
    kvs = IndexedDB::KVS.open('test_db')
    kvs['key1'] = 'value1'
    kvs.delete('key1')
    assert_nil(kvs['key1'])
  end

  def test_has_key
    kvs = IndexedDB::KVS.open('test_db')
    kvs['key1'] = 'value1'
    assert_equal(true, kvs.has_key?('key1'))
    assert_equal(false, kvs.has_key?('key2'))
  end

  def test_size
    kvs = IndexedDB::KVS.open('test_db')
    assert_equal(0, kvs.size)
    kvs['key1'] = 'value1'
    kvs['key2'] = 'value2'
    assert_equal(2, kvs.size)
  end

  def test_empty
    kvs = IndexedDB::KVS.open('test_db')
    assert_equal(true, kvs.empty?)
    kvs['key1'] = 'value1'
    assert_equal(false, kvs.empty?)
  end

  def test_clear
    kvs = IndexedDB::KVS.open('test_db')
    kvs['key1'] = 'value1'
    kvs['key2'] = 'value2'
    kvs.clear
    assert_equal(0, kvs.size)
    assert_nil(kvs['key1'])
  end

  def test_keys
    kvs = IndexedDB::KVS.open('test_db')
    kvs['alpha'] = 1
    kvs['beta'] = 2
    keys = kvs.keys
    assert_equal(2, keys.size)
    assert_equal(true, keys.include?('alpha'))
    assert_equal(true, keys.include?('beta'))
  end

  def test_values
    kvs = IndexedDB::KVS.open('test_db')
    kvs['a'] = 'x'
    kvs['b'] = 'y'
    vals = kvs.values
    assert_equal(2, vals.size)
    assert_equal(true, vals.include?('x'))
    assert_equal(true, vals.include?('y'))
  end

  def test_to_h
    kvs = IndexedDB::KVS.open('test_db')
    kvs['foo'] = 'bar'
    kvs['baz'] = 'qux'
    h = kvs.to_h
    assert_equal('bar', h['foo'])
    assert_equal('qux', h['baz'])
  end

  def test_store_hash_value
    kvs = IndexedDB::KVS.open('test_db')
    kvs['user'] = { 'name' => 'Alice', 'age' => 30 }
    result = kvs['user']
    assert_equal('Alice', result['name'])
    assert_equal(30, result['age'])
  end

  def test_store_array_value
    kvs = IndexedDB::KVS.open('test_db')
    kvs['items'] = [1, 2, 3]
    result = kvs['items']
    assert_equal([1, 2, 3], result)
  end

  def test_each
    kvs = IndexedDB::KVS.open('test_db')
    kvs['a'] = 1
    kvs['b'] = 2
    pairs = []
    kvs.each { |pair| pairs << pair }
    assert_equal(2, pairs.size)
  end
end
