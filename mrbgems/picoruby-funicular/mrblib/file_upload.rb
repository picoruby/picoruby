require 'rng'

module Funicular
  module FileUpload
    JS_HELPER_CODE = <<~JAVASCRIPT
      (function() {
        'use strict';
        // Avoid duplicate mounting
        if (window.funicularFormDataUpload) {
          return;
        }
        // FormData upload helper
        window.funicularFormDataUpload = function(url, fieldsObj, fileFieldName, fileRefId) {
          const formData = new FormData();
          for (const [key, value] of Object.entries(fieldsObj)) {
            formData.append(key, String(value));
          }
          if (fileFieldName && fileRefId !== null && fileRefId !== undefined) {
            const file = window.picorubyRefs[fileRefId];
            if (file) {
              formData.append(fileFieldName, file);
            }
          }
          return fetch(url, {
            method: 'PATCH',
            body: formData
          }).then(response => response.json());
        };
        console.log('Funicular helpers mounted');
      })();
    JAVASCRIPT

    def self.mount
      script = JS.document.createElement("script")
      script[:textContent] = JS_HELPER_CODE
      JS.document.body.appendChild(script)
      JS.document.body.removeChild(script)
    end

    # Select a file from input element and generate preview
    # @param input_id [String] DOM element ID of the file input
    # @param block [Proc] Callback with preview data URL (or nil if no file)
    def self.select_file_with_preview(input_id, &block)
      input = JS.document.getElementById(input_id)
      return block.call(nil, nil) unless input

      files = input[:files]
      if !files || files.length == 0
        return block.call(nil, nil)
      end

      file = files[0]

      # Use URL.createObjectURL instead of FileReader for preview
      # This is simpler and doesn't require async callbacks
      if file.nil?
        raise "Selected file is nil"
      else
        preview_url = JS.global[:URL].createObjectURL(file)
      end

      # Call the block with file and preview URL
      # @type var preview_url: String
      block.call(file, preview_url) if block
    end

    # Upload data with FormData (supports file upload)
    # @param url [String] Upload URL
    # @param fields [Hash] Form fields to include
    # @param file_field [String] Name of the file field
    # @param file [JS::Object] File object from input element
    # @param block [Proc] Callback with response data
    def self.upload_with_formdata(url, fields: {}, file_field: nil, file: nil, &block)
      # Store file and callback ID
      @callback_counters ||= []
      while true
        callback_id = RNG.random_int
        break unless @callback_counters.include?(callback_id)
      end
      @callback_counters << callback_id

      if file
        JS.global[:_tempFileForUpload] = file
      end

      # Build fields object as JSON string
      fields_json = JSON.generate(fields)

      # Call JavaScript helper
      # Note: The helper returns a Promise that resolves to JSON data (not Response)
      script = JS.document.createElement("script")
      script_code = <<~JS
        (function() {
          var fieldsObj = JSON.parse('#{fields_json.gsub("'", "\\\\'")}');
          var fileRefId = null;
          if (window._tempFileForUpload) {
            fileRefId = window.picorubyRefs.indexOf(window._tempFileForUpload);
          }
          // Call helper and store result as JSON string when Promise resolves
          window.funicularFormDataUpload(
            '#{url}',
            fieldsObj,
            #{file_field ? "'#{file_field}'" : 'null'},
            fileRefId
          ).then(function(data) {
            // Store as JSON string for easy Ruby parsing
            window._funicularUploadResult_#{callback_id} = JSON.stringify(data);
          }).catch(function(error) {
            window._funicularUploadResult_#{callback_id} = JSON.stringify({error: error.toString()});
          });
        })();
      JS
      script[:textContent] = script_code
      JS.document.body.appendChild(script)
      JS.document.body.removeChild(script)

      # Poll for result (simple polling approach)
      counter = 0
      json_str = nil
      while true
        counter += 1
        sleep 0.1
        json_str = JS.global[:"_funicularUploadResult_#{callback_id}"]
        break if json_str && !json_str.to_s.empty?
        if 100 < counter
          puts "[WARN] Timeout waiting for upload result. Return without calling block."
          return
        end
      end

      # Get JSON string from global variable (already retrieved above)
      result = JSON.parse(json_str.to_s)

      block.call(result) if block
      puts "[DEBUG] Block called"
    ensure
      @callback_counters.delete(callback_id) if @callback_counters
      if JS.global[:"_funicularUploadResult_#{callback_id}"]
        JS.global[:"_funicularUploadResult_#{callback_id}"] = nil
      end
      if JS.global[:_tempFileForUpload]
        JS.global[:_tempFileForUpload] = nil
      end
    end

    # Helper: Store file in global variable for later use
    def self.store_file(input_id, storage_key = '_selectedFile')
      input = JS.document.getElementById(input_id)
      return nil unless input

      files = input[:files]
      if !files || files.length == 0
        JS.global[storage_key] = nil
        return nil
      end

      file = files[0]
      JS.global[storage_key] = file
      file
    end

    # Helper: Retrieve stored file
    def self.retrieve_file(storage_key = '_selectedFile')
      file = JS.global[storage_key]
      JS.global[storage_key] = nil if file
      file
    end
  end
end
