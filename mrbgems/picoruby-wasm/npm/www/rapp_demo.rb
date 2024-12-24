require 'rapp'

include Rapp

class Counter < Component
  attr_reactive :num

  def initialize(selector)
    super(selector)
    @num = 0
  end

  def h(tag, attrs = {}, children = [])
    VNode.new(tag, attrs, children)
  end

  def render
    num = Root.counter.num
    new_vdom = h('div', {id: :counter}, [h('#text', {}, num.to_s)])

    patches = Differ.diff(@current_vdom, new_vdom)
    new_element = Patcher.apply(@element, patches)
    @element = new_element if new_element
    @current_vdom = new_vdom
  end
end

Root = Component.new

Root.add(:counter, Counter.new('#container #counter'))

Component.new(:button).on :click do
  Root.counter.num += 1
end
