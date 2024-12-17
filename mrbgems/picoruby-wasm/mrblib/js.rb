require 'json'

module JS
  class Object
    CALLBACKS = []
    def addEventListener(event, &block)
      _add_event_listener(event)
      CALLBACKS << block
    end
  end
end
