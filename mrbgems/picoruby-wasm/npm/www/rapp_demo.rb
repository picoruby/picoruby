require 'rapp'

include Rapp

class Counter < Component
  attr_reactive :num

  def initialize(selector)
    super(selector)
    @num = 0
  end

  def update_vdom
    num = Comps[:counter].num
    if num % 3 == 0 || num.to_s.include?('3')
      prop, num = {style: "color: red;"}, "aho"
    else
      prop = {}
    end
    h('div', {id: :counter}, [ h('b', prop, [ h('#text', {}, num)]) ])
  end
end

Comps.add(:counter, Counter.new('#container #counter'))

Component.new(:button).on :click do
  Comps[:counter].num += 1
end
