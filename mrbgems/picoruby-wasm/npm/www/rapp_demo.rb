require 'rapp'

include Rapp

class Counter < Component
  attr_reactive :num

  def initialize(selector)
    super(selector)
    @num = 0
  end

  def render
    query_eval do
      num = Root.counter.num
      style.color = (num % 3 == 0 || num.to_s.include?('3')) ? 'red' : 'black'
      innerHTML "<b>#{num}</b>"
    end
  end
end

Root = Component.new

Root.add(:counter, Counter.new('#container #counter'))

Component.new(:button).on :click do
  Root.counter.num += 1
end
