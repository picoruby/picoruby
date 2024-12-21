require 'json'

module JS
  class Object
    CALLBACKS = {}
    def addEventListener(event_type, &block)
      callback_id = block.object_id
      _add_event_listener(callback_id, event_type)
      CALLBACKS[callback_id] = block
      return callback_id # When removeEventListener, we need to pass this id
    end
  end
end
