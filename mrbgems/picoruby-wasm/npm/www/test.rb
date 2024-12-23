require 'rapp'

class Counter < Rapp::Component
  def initialize
    @count = 0
  end

  def up
    @count += 1
  end

  def render
    count = @count
    div :main do
      div :sub do
        innerHTML "<b>#{count.to_s}</b>"
      end
    end
  end
end

$counter = Counter.new

JS.global[:document].getElementById('button').addEventListener('click') do |_e|
  $counter.up
  $counter.render
end
