class VDOMTest < Picotest::Test
  # Element tests
  def test_element_basic_creation
    element = Funicular::VDOM::Element.new('div')
    assert_equal(:element, element.type)
    assert_equal('div', element.tag)
    assert_equal({}, element.props)
    assert_equal([], element.children)
    assert_nil(element.key)
  end

  def test_element_with_props
    element = Funicular::VDOM::Element.new('div', {class: 'foo', id: 'bar'})
    assert_equal('div', element.tag)
    assert_equal({class: 'foo', id: 'bar'}, element.props)
  end

  def test_element_with_children
    child1 = Funicular::VDOM::Element.new('span')
    child2 = Funicular::VDOM::Text.new('hello')
    element = Funicular::VDOM::Element.new('div', {}, [child1, child2])
    assert_equal(2, element.children.length)
    assert_equal(child1, element.children[0])
    assert_equal(child2, element.children[1])
  end

  def test_element_key_extraction
    element = Funicular::VDOM::Element.new('div', {key: 'test-key', class: 'foo'})
    assert_equal('test-key', element.key)
    assert_equal({class: 'foo'}, element.props)
    # Key should be removed from props
    assert_nil(element.props[:key])
  end

  def test_element_key_with_numeric_value
    element = Funicular::VDOM::Element.new('div', {key: 123, class: 'foo'})
    assert_equal(123, element.key)
    assert_equal({class: 'foo'}, element.props)
  end

  def test_element_normalize_children_with_arrays
    child1 = Funicular::VDOM::Element.new('span')
    child2 = Funicular::VDOM::Element.new('p')
    child3 = Funicular::VDOM::Text.new('text')
    # Nested arrays should be flattened
    element = Funicular::VDOM::Element.new('div', {}, [[child1, child2], child3])
    assert_equal(3, element.children.length)
    assert_equal(child1, element.children[0])
    assert_equal(child2, element.children[1])
    assert_equal(child3, element.children[2])
  end

  def test_element_normalize_children_with_strings
    element = Funicular::VDOM::Element.new('div', {}, ['hello', 'world'])
    assert_equal(2, element.children.length)
    assert_equal('hello', element.children[0])
    assert_equal('world', element.children[1])
  end

  def test_element_normalize_children_with_nil
    child = Funicular::VDOM::Element.new('span')
    # nil values should be skipped
    element = Funicular::VDOM::Element.new('div', {}, [child, nil, 'text'])
    assert_equal(2, element.children.length)
    assert_equal(child, element.children[0])
    assert_equal('text', element.children[1])
  end

  def test_element_normalize_children_with_numbers
    element = Funicular::VDOM::Element.new('div', {}, [123, 456])
    assert_equal(2, element.children.length)
    assert_equal('123', element.children[0])
    assert_equal('456', element.children[1])
  end

  def test_element_equality
    element1 = Funicular::VDOM::Element.new('div', {class: 'foo'}, ['hello'])
    element2 = Funicular::VDOM::Element.new('div', {class: 'foo'}, ['hello'])
    assert_equal(element1, element2)
  end

  def test_element_inequality_different_tag
    element1 = Funicular::VDOM::Element.new('div')
    element2 = Funicular::VDOM::Element.new('span')
    assert_not_equal(element1, element2)
  end

  def test_element_inequality_different_props
    element1 = Funicular::VDOM::Element.new('div', {class: 'foo'})
    element2 = Funicular::VDOM::Element.new('div', {class: 'bar'})
    assert_not_equal(element1, element2)
  end

  # Text tests
  def test_text_creation
    text = Funicular::VDOM::Text.new('hello')
    assert_equal(:text, text.type)
    assert_equal('hello', text.content)
  end

  def test_text_with_number
    text = Funicular::VDOM::Text.new(123)
    assert_equal('123', text.content)
  end

  def test_text_equality
    text1 = Funicular::VDOM::Text.new('hello')
    text2 = Funicular::VDOM::Text.new('hello')
    assert_equal(text1, text2)
  end

  def test_text_inequality
    text1 = Funicular::VDOM::Text.new('hello')
    text2 = Funicular::VDOM::Text.new('world')
    assert_not_equal(text1, text2)
  end

  # Component tests
  def test_component_creation
    component = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    assert_equal(:component, component.type)
    assert_equal(String, component.component_class)
    assert_equal({foo: 'bar'}, component.props)
    assert_nil(component.instance)
    assert_nil(component.key)
  end

  def test_component_key_extraction
    component = Funicular::VDOM::Component.new(String, {key: 'comp-key', foo: 'bar'})
    assert_equal('comp-key', component.key)
    assert_equal({foo: 'bar'}, component.props)
    # Key should be removed from props
    assert_nil(component.props[:key])
  end

  def test_component_equality
    component1 = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    component2 = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    assert_equal(component1, component2)
  end

  def test_component_inequality_different_class
    component1 = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    component2 = Funicular::VDOM::Component.new(Array, {foo: 'bar'})
    assert_not_equal(component1, component2)
  end

  def test_component_inequality_different_props
    component1 = Funicular::VDOM::Component.new(String, {foo: 'bar'})
    component2 = Funicular::VDOM::Component.new(String, {foo: 'baz'})
    assert_not_equal(component1, component2)
  end

  # VDOM helper method tests
  def test_create_element
    element = Funicular::VDOM.create_element('div', {class: 'foo'}, 'hello', 'world')
    assert_equal('div', element.tag)
    assert_equal({class: 'foo'}, element.props)
    assert_equal(2, element.children.length)
    assert_equal('hello', element.children[0])
    assert_equal('world', element.children[1])
  end

  def test_create_text
    text = Funicular::VDOM.create_text('hello')
    assert_equal('hello', text.content)
  end
end
