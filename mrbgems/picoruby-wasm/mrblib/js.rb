require 'machine'
require 'json'

module JS
  def self.document
    global[:document]
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

    def to_binary
      callback_id = self.object_id
      _to_binary_and_suspend(callback_id)
      result = $promise_responses[callback_id]
      $promise_responses.delete(callback_id)
      result.to_s
    end

    # Promise#then support
    # This allows calling .then on Promise objects returned from JavaScript
    def then(&block)
      # Store promise in global variable temporarily
      callback_id = block.object_id
      JS.global[:"_tempPromise_#{callback_id}"] = self

      # Create a JavaScript callback that will store the result
      script = JS.document.createElement("script")
      script[:textContent] = <<~JAVASCRIPT
        (function() {
          var promise = window._tempPromise_#{callback_id};
          if (promise && typeof promise.then === 'function') {
            promise.then(function(result) {
              var resultId = window.picorubyRefs.push(result) - 1;
              window._promiseResult_#{callback_id} = resultId;
            }).catch(function(error) {
              console.error('Promise rejected:', error);
              window._promiseResult_#{callback_id} = -1;
            });
          } else {
            window._promiseResult_#{callback_id} = -1;
          }
        })();
      JAVASCRIPT
      JS.document.body.appendChild(script)
      JS.document.body.removeChild(script)

      # Poll for result
      sleep 0.05 until JS.global[:"_promiseResult_#{callback_id}"]

      result_id = JS.global[:"_promiseResult_#{callback_id}"].to_poro
      JS.global[:"_promiseResult_#{callback_id}"] = nil
      JS.global[:"_tempPromise_#{callback_id}"] = nil

      # @type var result_id: Integer
      if 0 <= result_id
        # Create JS::Object from result_id
        result_obj = JS.global[:picorubyRefs][result_id]
        # @type var block: Proc
        block.call(result_obj) if block
      end
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
      when Array
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
