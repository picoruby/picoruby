require 'machine'
require 'json'

module JS
  def self.document
    document = global[:document]
    if document.is_a?(JS::Element)
      document
    else
      raise 'Document object is not available'
    end
  end

  def self.generic_callbacks
    callbacks = global[:picorubyGenericCallbacks]
    if callbacks.is_a?(JS::Object)
      return callbacks
    else
      raise 'Generic callbacks object is not available'
    end
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

    def self.register_callback(name, &block)
      callback_id = block.object_id
      _register_callback(callback_id, name)
      CALLBACKS[callback_id] = block
      callback_id
    end

    def self.removeEventListener(callback_id)
      return false unless callback_id
      begin
        result = JS.global._js_remove_event_listener_wrapper(callback_id)
        CALLBACKS.delete(callback_id) if result
        result
      rescue
        # Ignore errors if wrapper doesn't exist
        CALLBACKS.delete(callback_id)
        false
      end
    end

    def fetch(url, options = nil, &block)
      raise ArgumentError, "JS::Object#fetch requires a block: use `fetch(url) { |resp| ... }`" unless block
      callback_id = block.object_id
      if options
        options_json = JSON.generate(options)
        _fetch_with_options_and_suspend(url, options_json, callback_id)
      else
        _fetch_and_suspend(url, callback_id)
      end
      block.call($promise_responses[callback_id])
      $promise_responses.delete(callback_id)
    end

    def setTimeout(delay_ms, &block)
      callback_id = block.object_id
      CALLBACKS[callback_id] = block
      timer_id = _set_timeout(callback_id, delay_ms)
      callback_id  # Return callback_id to use with clearTimeout
    end

    def clearTimeout(callback_id)
      return false unless callback_id
      success = _clear_timeout(callback_id)
      CALLBACKS.delete(callback_id) if success
      success
    end

  end

  # Composite-type subclasses. wrap_ref_as_js_object in js.c picks the right
  # class based on the JS runtime type of the wrapped value.

  class Array < Object
    include Enumerable

    # Iterate over the wrapped JS array, yielding each element converted via
    # js_ref_to_ruby_value (primitives become Ruby native values).
    # Uses indexed access directly; calling #to_a here would infinite-loop
    # via Enumerable#to_a -> #each.
    def each(&block)
      return self unless block
      i = 0
      n = length
      while i < n
        yield self[i]
        i += 1
      end
      self
    end

    def size
      length
    end
  end

  class Function < Object
    # #call is defined in C on class_JS_Function and invokes the wrapped JS
    # function value directly (no `this` binding).
  end

  class Response < Object
    # Read the response body as a binary String. Suspends the current Ruby
    # task until the underlying ArrayBuffer is materialized.
    def to_binary
      callback_id = self.object_id
      _to_binary_and_suspend(callback_id)
      result = $promise_responses[callback_id]
      $promise_responses.delete(callback_id)
      result.to_s
    end
  end

  class Event < Object
    # #preventDefault and #stopPropagation are defined in C on class_JS_Event.
  end

  class Element < Object
    # DOM element / Document. Methods (createElement, appendChild,
    # setAttribute, etc.) are defined in C on class_JS_Element.
  end

  class Promise < Object
    # Await a JS Promise. Suspends the current Ruby task until the Promise
    # resolves and returns the resolved value (auto-converted to a Ruby
    # native value if it is a primitive, otherwise wrapped as JS::Object).
    def await
      callback_id = self.object_id
      _await_and_suspend(callback_id)
      result = $promise_responses[callback_id]
      $promise_responses.delete(callback_id)
      result
    end

    # Promise#then - suspends task until Promise resolves, then calls block.
    def then(&block)
      result = await
      block.call(result) if block
      result
    end
  end

  module Bridge
    # Deep convert Ruby value to JS value
    # @param value [Object] Ruby object (Hash, Array, or primitive)
    # @return [JS::Object | Object] JS object or primitive value
    def self.to_js(value)
      case value
      when Hash
        hash_to_js_object(value)
      when ::Array
        array_to_js_array(value)
      else
        value
      end
    end

    private

    # Convert Ruby Hash to JS object
    # @param hash [Hash] Ruby hash
    # @return [JS::Object] JS object
    def self.hash_to_js_object(hash)
      js_obj = JS.global.create_object
      hash.each do |key, val|
        js_obj[key.to_s.to_sym] = to_js(val)
      end
      js_obj
    end

    # Convert Ruby Array to JS array
    # @param array [Array] Ruby array
    # @return [JS::Object] JS array
    def self.array_to_js_array(array)
      js_array = JS.global.create_array
      array.each { |item| js_array.push(to_js(item)) }
      js_array
    end
  end
end
