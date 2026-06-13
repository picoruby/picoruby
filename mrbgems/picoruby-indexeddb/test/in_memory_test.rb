class IndexedDBInMemoryTest < Picotest::Test
  def setup
    IndexedDB::InMemoryDatabase.stores = {}
    IndexedDB::InMemoryDatabase.meta = {}
  end

  def test_open_creates_database
    db = IndexedDB::InMemoryDatabase.open('test_db', 1)
    assert_equal('test_db', db.name)
    assert_equal(1, db.version)
  end

  def test_create_store_in_upgrade_block
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, old_v, new_v|
      d.create_store('users')
    end
    assert_equal(true, db.has_store?('users'))
  end

  def test_store_names
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('users')
      d.create_store('posts')
    end
    names = db.store_names
    assert_equal(true, names.include?('users'))
    assert_equal(true, names.include?('posts'))
  end

  def test_store_put_and_get
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    store = db.store('items')
    store.put('value1', key: 'key1')
    assert_equal('value1', store.get('key1'))
  end

  def test_store_with_key_path
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('users', key_path: 'id')
    end
    store = db.store('users')
    store.put({ 'id' => 1, 'name' => 'Alice' })
    result = store.get(1)
    assert_equal('Alice', result['name'])
  end

  def test_store_with_auto_increment
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items', auto_increment: true)
    end
    store = db.store('items')
    key1 = store.put('first')
    key2 = store.put('second')
    assert_equal(1, key1)
    assert_equal(2, key2)
  end

  def test_store_delete
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    store = db.store('items')
    store.put('value', key: 'key1')
    store.delete('key1')
    assert_nil(store.get('key1'))
  end

  def test_store_count
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    store = db.store('items')
    assert_equal(0, store.count)
    store.put('a', key: 'k1')
    store.put('b', key: 'k2')
    assert_equal(2, store.count)
  end

  def test_store_clear
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    store = db.store('items')
    store.put('a', key: 'k1')
    store.put('b', key: 'k2')
    store.clear
    assert_equal(0, store.count)
  end

  def test_store_keys
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    store = db.store('items')
    store.put('x', key: 'alpha')
    store.put('y', key: 'beta')
    keys = store.keys
    assert_equal(true, keys.include?('alpha'))
    assert_equal(true, keys.include?('beta'))
  end

  def test_store_to_a
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    store = db.store('items')
    store.put('val1', key: 'k1')
    store.put('val2', key: 'k2')
    arr = store.to_a
    assert_equal(2, arr.size)
  end

  def test_delete_store
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('temp')
    end
    assert_equal(true, db.has_store?('temp'))

    db2 = IndexedDB::InMemoryDatabase.open('test_db', 2) do |d, _, _|
      d.delete_store('temp')
    end
    assert_equal(false, db2.has_store?('temp'))
  end

  def test_batch_put
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    db.batch('items') do |tx|
      tx['items'].put('a', key: 'k1')
      tx['items'].put('b', key: 'k2')
    end
    store = db.store('items')
    assert_equal('a', store.get('k1'))
    assert_equal('b', store.get('k2'))
  end

  def test_batch_delete
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      d.create_store('items')
    end
    store = db.store('items')
    store.put('val', key: 'k1')

    db.batch('items') do |tx|
      tx['items'].delete('k1')
    end
    assert_nil(store.get('k1'))
  end

  def test_close_database
    db = IndexedDB::InMemoryDatabase.open('test_db', 1)
    assert_equal(false, db.closed?)
    db.close
    assert_equal(true, db.closed?)
  end

  def test_create_index
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      store = d.create_store('users')
      store.create_index('by_email', 'email', unique: true)
    end
    store = db.store('users')
    assert_equal(true, store.index_names.include?('by_email'))
  end

  def test_index_get
    db = IndexedDB::InMemoryDatabase.open('test_db', 1) do |d, _, _|
      store = d.create_store('users')
      store.create_index('by_email', 'email')
    end
    store = db.store('users')
    store.put({ 'id' => 1, 'email' => 'alice@example.com', 'name' => 'Alice' }, key: 1)
    store.put({ 'id' => 2, 'email' => 'bob@example.com', 'name' => 'Bob' }, key: 2)

    idx = store.index('by_email')
    result = idx.get('alice@example.com')
    assert_equal('Alice', result['name'])
  end
end
