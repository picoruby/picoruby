class VDOMDifferTest < Picotest::Test
  def setup
    @differ = Funicular::VDOM::Differ
  end

  # Basic diff tests
  def test_diff_returns_replace_when_old_is_nil
    new_node = Funicular::VDOM::Element.new('div')
    patches = @differ.diff(nil, new_node)
    assert_equal([[:replace, new_node]], patches)
  end

  def test_diff_returns_remove_when_new_is_nil
    old_node = Funicular::VDOM::Element.new('div')
    patches = @differ.diff(old_node, nil)
    assert_equal([[:remove]], patches)
  end

  def test_diff_text_node_no_change
    old_node = Funicular::VDOM::Text.new('hello')
    new_node = Funicular::VDOM::Text.new('hello')
    patches = @differ.diff(old_node, new_node)
    assert_equal([], patches)
  end

  def test_diff_text_node_changed
    old_node = Funicular::VDOM::Text.new('hello')
    new_node = Funicular::VDOM::Text.new('world')
    patches = @differ.diff(old_node, new_node)
    assert_equal([[:replace, new_node]], patches)
  end

  def test_diff_element_different_tag
    old_node = Funicular::VDOM::Element.new('div')
    new_node = Funicular::VDOM::Element.new('span')
    patches = @differ.diff(old_node, new_node)
    assert_equal([[:replace, new_node]], patches)
  end

  def test_diff_element_same_tag_no_props_change
    old_node = Funicular::VDOM::Element.new('div', {class: 'foo'})
    new_node = Funicular::VDOM::Element.new('div', {class: 'foo'})
    patches = @differ.diff(old_node, new_node)
    assert_equal([], patches)
  end

  def test_diff_element_props_changed
    old_node = Funicular::VDOM::Element.new('div', {class: 'foo'})
    new_node = Funicular::VDOM::Element.new('div', {class: 'bar'})
    patches = @differ.diff(old_node, new_node)
    assert_equal([[:props, {class: 'bar'}]], patches)
  end

  def test_diff_element_props_added
    old_node = Funicular::VDOM::Element.new('div')
    new_node = Funicular::VDOM::Element.new('div', {class: 'foo'})
    patches = @differ.diff(old_node, new_node)
    assert_equal([[:props, {class: 'foo'}]], patches)
  end

  def test_diff_element_props_removed
    old_node = Funicular::VDOM::Element.new('div', {class: 'foo'})
    new_node = Funicular::VDOM::Element.new('div')
    patches = @differ.diff(old_node, new_node)
    assert_equal([[:props, {class: nil}]], patches)
  end

  # Children diff tests - index-based (no keys)
  # Test via Element diff since diff_children is private
  def test_diff_children_by_index_no_change
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li'),
      Funicular::VDOM::Element.new('li')
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li'),
      Funicular::VDOM::Element.new('li')
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal([], patches)
  end

  def test_diff_children_by_index_child_changed
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {class: 'foo'}),
      Funicular::VDOM::Element.new('li')
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {class: 'bar'}),
      Funicular::VDOM::Element.new('li')
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal(1, patches.length)
    assert_equal(0, patches[0][0]) # Child index 0
    assert_equal([[:props, {class: 'bar'}]], patches[0][1])
  end

  def test_diff_children_by_index_child_added
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li')
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li'),
      Funicular::VDOM::Element.new('li')
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal(1, patches.length)
    assert_equal(1, patches[0][0]) # Child index 1
    new_li = new_element.children[1]
    assert_equal([[:replace, new_li]], patches[0][1])
  end

  def test_diff_children_by_index_child_removed
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li'),
      Funicular::VDOM::Element.new('li')
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li')
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal(1, patches.length)
    assert_equal(1, patches[0][0]) # Child index 1
    assert_equal([[:remove]], patches[0][1])
  end

  # Children diff tests - key-based
  def test_diff_children_with_keys_no_change
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('li', {key: 'b'})
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('li', {key: 'b'})
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal([], patches)
  end

  def test_diff_children_with_keys_reordered
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a', class: 'first'}),
      Funicular::VDOM::Element.new('li', {key: 'b', class: 'second'})
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'b', class: 'second'}),
      Funicular::VDOM::Element.new('li', {key: 'a', class: 'first'})
    ])
    patches = @differ.diff(old_element, new_element)
    # No patches should be generated since the elements themselves haven't changed
    # (just their positions, which will be handled by remove+insert)
    assert_equal([], patches)
  end

  def test_diff_children_with_keys_element_added
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('li', {key: 'b'})
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('li', {key: 'c'}),
      Funicular::VDOM::Element.new('li', {key: 'b'})
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal(1, patches.length)
    assert_equal(1, patches[0][0]) # Child index 1 (where 'c' should be inserted)
    new_c = new_element.children[1]
    assert_equal([[:replace, new_c]], patches[0][1])
  end

  def test_diff_children_with_keys_element_removed
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('li', {key: 'b'}),
      Funicular::VDOM::Element.new('li', {key: 'c'})
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('li', {key: 'c'})
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal(1, patches.length)
    assert_equal(1, patches[0][0]) # Child index 1 (where 'b' was)
    assert_equal([[:remove]], patches[0][1])
  end

  def test_diff_children_with_keys_element_props_changed
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a', class: 'foo'}),
      Funicular::VDOM::Element.new('li', {key: 'b', class: 'bar'})
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a', class: 'baz'}),
      Funicular::VDOM::Element.new('li', {key: 'b', class: 'bar'})
    ])
    patches = @differ.diff(old_element, new_element)
    assert_equal(1, patches.length)
    assert_equal(0, patches[0][0]) # Child index 0
    assert_equal([[:props, {class: 'baz'}]], patches[0][1])
  end

  def test_diff_children_with_keys_mixed_with_without_keys
    old_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('span') # No key
    ])
    new_element = Funicular::VDOM::Element.new('ul', {}, [
      Funicular::VDOM::Element.new('li', {key: 'a'}),
      Funicular::VDOM::Element.new('p') # No key, different tag
    ])
    patches = @differ.diff(old_element, new_element)
    # Should detect that child index 1 changed from span to p
    assert_equal(1, patches.length)
    assert_equal(1, patches[0][0])
    assert_equal([[:replace, new_element.children[1]]], patches[0][1])
  end

  # Component diff tests
  def test_diff_component_different_class
    old_node = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    new_node = Funicular::VDOM::Component.new(Array, {foo: 'bar'})
    patches = @differ.diff(old_node, new_node)
    assert_equal([[:replace, new_node]], patches)
  end

  def test_diff_component_same_class_same_props
    old_node = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    new_node = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    patches = @differ.diff(old_node, new_node)
    assert_equal([], patches)
  end

  def test_diff_component_same_class_different_props_without_preserve
    old_node = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    new_node = Funicular::VDOM::Component.new(String, {foo: 'baz'})
    patches = @differ.diff(old_node, new_node)
    assert_equal([[:replace, new_node]], patches)
  end
end
