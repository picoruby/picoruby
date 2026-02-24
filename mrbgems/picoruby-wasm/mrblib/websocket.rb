module JS
  class WebSocket
    def onopen(&block)
      callback_id = block.object_id
      _set_onopen(callback_id)
      JS::Object::CALLBACKS[callback_id] = block
    end

    def onmessage(&block)
      callback_id = block.object_id
      _set_onmessage(callback_id)
      JS::Object::CALLBACKS[callback_id] = block
    end

    def onerror(&block)
      callback_id = block.object_id
      _set_onerror(callback_id)
      JS::Object::CALLBACKS[callback_id] = block
    end

    def onclose(&block)
      callback_id = block.object_id
      _set_onclose(callback_id)
      JS::Object::CALLBACKS[callback_id] = block
    end

    def connecting?
      ready_state == CONNECTING
    end

    def open?
      ready_state == OPEN
    end

    def closing?
      ready_state == CLOSING
    end

    def closed?
      ready_state == CLOSED
    end
  end
end
