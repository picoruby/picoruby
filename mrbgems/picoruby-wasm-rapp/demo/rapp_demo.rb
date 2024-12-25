require 'rapp'

class Counter < Rapp::Component
  attr_reactive :num

  def initialize(selector)
    super(selector)
    @num = 0
  end

  def update_vdom
    if @num % 3 == 0 || @num.to_s.include?('3')
      prop, word = {style: "color: red;"}, "aho"
    else
      prop, word = {}, num
    end
    h('div', {id: :counter}, [ h('b', prop, [ h('#text', {}, word)]) ])
  end
end

Rapp::Comps.add(:counter, Counter.new('#container #counter'))

Rapp::Component.new(:button).on :click do
  Rapp::Comps[:counter].num += 1
end
