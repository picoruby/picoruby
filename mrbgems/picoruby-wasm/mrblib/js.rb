require 'json'

module JS
  class Object
    CALLBACKS = {}
    $responses = {}

    def addEventListener(event_type, &block)
      callback_id = block.object_id
      _add_event_listener(callback_id, event_type)
      CALLBACKS[callback_id] = block
      return callback_id # When removeEventListener, we need to pass this id
    end

    def fetch(url, &block)
      callback_id = block.object_id
      _fetch_and_suspend(url, callback_id)
      # resumed by calback
      block.call($responses[callback_id])
      $responses.delete(callback_id)
    end
  end
end
