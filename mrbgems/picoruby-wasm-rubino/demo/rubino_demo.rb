require 'rubino'

class Counter < Rubino::Component
  attr_reactive :num

  def initialize(selector)
    super(selector)
    @num = 0
  end

  def update_vdom
    if @num % 3 == 0 || @num.to_s.include?('3')
      prop, word = {style: "color: red; font-size: 50px;"}, "aho"
    else
      prop, word = {}, num
    end
    h('b', prop, [ h('#text', {}, word)])
  end
end

Rubino::Comps.add(:counter, Counter.new('#container #counter'))

Rubino::Component.new(:button).on :click do
  Rubino::Comps[:counter].num += 1
end
