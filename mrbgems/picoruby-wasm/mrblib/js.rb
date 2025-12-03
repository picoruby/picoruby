require 'machine'
require 'json'

module JS
  def self.document
    $js_document ||= global[:document]
  end

  class Object
    CALLBACKS = {}
    $promise_responses = {}
    $js_events = {}

    def addEventListener(event_type, &block)
      callback_id = block.object_id
      _add_event_listener(callback_id, event_type)
      CALLBACKS[callback_id] = block
      callback_id
    end

    def removeEventListener(callback_id)
      return false unless callback_id
      result = _removeEventListener(callback_id)
      CALLBACKS.delete(callback_id) if result
      result
    end

    def fetch(url, &block)
      callback_id = block.object_id
      _fetch_and_suspend(url, callback_id)
      block.call($promise_responses[callback_id])
      $promise_responses.delete(callback_id)
    end

    def to_binary
      callback_id = self.object_id
      _to_binary_and_suspend(callback_id)
      result = $promise_responses[callback_id]
      $promise_responses.delete(callback_id)
      result.to_s
    end

    def preventDefault
      self[:preventDefault].call(self)
    end

    def stopPropagation
      self[:stopPropagation].call(self)
    end
  end
end
