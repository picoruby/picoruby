# Mock JS module for testing without picoruby-wasm dependency
module JS
  class Object
  end
end

class VDOMPatcherTest < Picotest::Test
  # Mock childNodes that behaves like JS::Object
  class MockChildNodes < JS::Object
    attr_reader :elements

    def initialize(elements)
      @elements = elements
    end

    def to_a
      @elements
    end
  end

  # Mock DOM classes for testing without JS dependency
  class MockDocument
    def createElement(tag)
      MockElement.new(tag)
    end

    def createTextNode(text)
      MockTextNode.new(text)
    end

    def [](key)
      nil # activeElement and other properties
    end
  end

  class MockElement
    attr_accessor :tag_name, :attributes, :children, :parent_element, :properties

    def initialize(tag)
      @tag_name = tag
      @attributes = {}
      @children = []
      @parent_element = nil
      @properties = {}
    end

    def setAttribute(key, value)
      @attributes[key] = value
    end

    def removeAttribute(key)
      @attributes.delete(key)
    end

    def appendChild(child)
      @children << child
      child.parent_element = self if child.respond_to?(:parent_element=)
      child
    end

    def removeChild(child)
      @children.delete(child)
      child.parent_element = nil if child.respond_to?(:parent_element=)
      child
    end

    def replaceChild(new_child, old_child)
      index = @children.index(old_child)
      if index
        @children[index] = new_child
        new_child.parent_element = self if new_child.respond_to?(:parent_element=)
        old_child.parent_element = nil if old_child.respond_to?(:parent_element=)
      end
      new_child
    end

    def [](key)
      case key.to_s
      when 'tagName'
        @tag_name
      when 'childNodes'
        VDOMPatcherTest::MockChildNodes.new(@children)
      else
        @properties[key.to_s]
      end
    end

    def []=(key, value)
      @properties[key.to_s] = value
    end

    def parentElement
      @parent_element
    end

    def is_a?(klass)
      # Mock JS::Object check
      if klass.to_s == 'JS::Object'
        false # We're not actually a JS::Object
      else
        super
      end
    end
  end

  class MockTextNode
    attr_accessor :text_content, :parent_element

    def initialize(text)
      @text_content = text
      @parent_element = nil
    end

    def parentElement
      @parent_element
    end
  end

  def setup
    @doc = MockDocument.new
    @patcher = Funicular::VDOM::Patcher.new(@doc)
  end

  # Test props update
  def test_update_props_add_attribute
    element = @doc.createElement('div')
    patches = [[:props, {class: 'foo', id: 'bar'}]]

    @patcher.apply(element, patches)

    assert_equal('foo', element.attributes['class'])
    assert_equal('bar', element.attributes['id'])
  end

  def test_update_props_change_attribute
    element = @doc.createElement('div')
    element.setAttribute('class', 'old')
    patches = [[:props, {class: 'new'}]]

    @patcher.apply(element, patches)

    assert_equal('new', element.attributes['class'])
  end

  def test_update_props_remove_attribute
    element = @doc.createElement('div')
    element.setAttribute('class', 'foo')
    patches = [[:props, {class: nil}]]

    @patcher.apply(element, patches)

    assert_nil(element.attributes['class'])
  end

  def test_update_props_skip_event_handlers
    element = @doc.createElement('div')
    patches = [[:props, {onclick: 'handler', class: 'foo'}]]

    @patcher.apply(element, patches)

    # Event handler should be skipped
    assert_nil(element.attributes['onclick'])
    # But other props should be set
    assert_equal('foo', element.attributes['class'])
  end

  # Test element creation from VDOM
  def test_create_text_element
    text_vnode = Funicular::VDOM::Text.new('hello')
    # Access private method for testing
    element = @patcher.send(:create_element, text_vnode)

    assert(element.is_a?(MockTextNode))
    assert_equal('hello', element.text_content)
  end

  def test_create_simple_element
    vnode = Funicular::VDOM::Element.new('div', {class: 'foo'})
    element = @patcher.send(:create_element, vnode)

    assert(element.is_a?(MockElement))
    assert_equal('div', element.tag_name)
    assert_equal('foo', element.attributes['class'])
    assert_equal(0, element.children.length)
  end

  def test_create_element_with_text_children
    vnode = Funicular::VDOM::Element.new('p', {}, ['hello', 'world'])
    element = @patcher.send(:create_element, vnode)

    assert_equal('p', element.tag_name)
    assert_equal(2, element.children.length)
    assert(element.children[0].is_a?(MockTextNode))
    assert_equal('hello', element.children[0].text_content)
    assert(element.children[1].is_a?(MockTextNode))
    assert_equal('world', element.children[1].text_content)
  end

  def test_create_element_with_element_children
    child1 = Funicular::VDOM::Element.new('span')
    child2 = Funicular::VDOM::Element.new('strong')
    parent = Funicular::VDOM::Element.new('div', {}, [child1, child2])

    element = @patcher.send(:create_element, parent)

    assert_equal('div', element.tag_name)
    assert_equal(2, element.children.length)
    assert(element.children[0].is_a?(MockElement))
    assert_equal('span', element.children[0].tag_name)
    assert(element.children[1].is_a?(MockElement))
    assert_equal('strong', element.children[1].tag_name)
  end

  def test_create_element_with_nested_children
    text = Funicular::VDOM::Text.new('text')
    span = Funicular::VDOM::Element.new('span', {}, [text])
    div = Funicular::VDOM::Element.new('div', {}, [span])

    element = @patcher.send(:create_element, div)

    assert_equal('div', element.tag_name)
    assert_equal(1, element.children.length)
    span_element = element.children[0]
    assert_equal('span', span_element.tag_name)
    assert_equal(1, span_element.children.length)
    assert(span_element.children[0].is_a?(MockTextNode))
    assert_equal('text', span_element.children[0].text_content)
  end

  # Test remove patch
  def test_remove_child
    parent = @doc.createElement('div')
    child = @doc.createElement('span')
    parent.appendChild(child)

    assert_equal(1, parent.children.length)

    patches = [[:remove]]
    @patcher.apply(child, patches)

    assert_equal(0, parent.children.length)
    assert_nil(child.parent_element)
  end

  # Test child index patches
  def test_apply_child_patch_add_new_child
    parent = @doc.createElement('ul')
    existing_child = @doc.createElement('li')
    parent.appendChild(existing_child)

    new_child_vnode = Funicular::VDOM::Element.new('li', {class: 'new'})
    patches = [[1, [[:replace, new_child_vnode]]]]

    @patcher.apply(parent, patches)

    assert_equal(2, parent.children.length)
    assert_equal('new', parent.children[1].attributes['class'])
  end

  def test_apply_child_patch_update_existing_child
    parent = @doc.createElement('ul')
    child = @doc.createElement('li')
    child.setAttribute('class', 'old')
    parent.appendChild(child)

    patches = [[0, [[:props, {class: 'updated'}]]]]

    @patcher.apply(parent, patches)

    assert_equal(1, parent.children.length)
    assert_equal('updated', parent.children[0].attributes['class'])
  end

  def test_apply_empty_patches
    element = @doc.createElement('div')
    result = @patcher.apply(element, [])

    assert_equal(element, result)
  end

  def test_create_element_from_string
    element = @patcher.send(:create_element, 'hello')

    assert(element.is_a?(MockTextNode))
    assert_equal('hello', element.text_content)
  end
end
