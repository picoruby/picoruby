module Funicular
  class FormBuilder
    attr_reader :component, :model_key, :options

    def initialize(component, model_key, options = {})
      @component = component
      @model_key = model_key
      @options = options
      @error_class = options[:error_class] || "text-red-600 text-sm mt-1"
      @field_error_class = options[:field_error_class] || "border-red-500"
    end

    # Generic field builder for input elements
    def build_field(field_name, field_type, field_options = {})
      field_key = field_name.to_s
      full_key = "#{@model_key}.#{field_key}"

      # Get current value from state
      value = get_nested_value(@component.state, full_key) || ""

      # Build input change handler
      on_input = ->(event) do
        new_value = event.target[:value]
        set_nested_value(@model_key, field_key, new_value)
      end

      # Check for errors
      error_message = @component.state.errors ? @component.state.errors[field_key.to_sym] : nil
      has_error = !error_message.nil?

      # Merge CSS classes (add error class if error exists)
      css_class = field_options[:class]
      css_class = css_class.to_s if css_class
      css_class ||= ""

      if has_error
        if css_class.empty?
          css_class = @field_error_class
        else
          css_class = "#{css_class} #{@field_error_class}"
        end
      end

      # Build field attributes
      attrs = {
        type: field_type,
        value: value,
        oninput: on_input
      }.merge(field_options.reject { |k, _| k == :class })

      # Add class attribute if not empty
      attrs[:class] = css_class unless css_class.empty?

      # Render field + error message
      @component.div do
        @component.input(attrs)
        if has_error
          @component.div(class: @error_class) { error_message }
        end
      end
    end

    # Public field methods
    def text_field(field_name, options = {})
      build_field(field_name, "text", options)
    end

    def password_field(field_name, options = {})
      build_field(field_name, "password", options)
    end

    def email_field(field_name, options = {})
      build_field(field_name, "email", options)
    end

    def number_field(field_name, options = {})
      build_field(field_name, "number", options)
    end

    def textarea(field_name, options = {})
      field_key = field_name.to_s
      full_key = "#{@model_key}.#{field_key}"

      value = get_nested_value(@component.state, full_key) || ""

      on_input = ->(event) do
        new_value = event.target[:value]
        set_nested_value(@model_key, field_key, new_value)
      end

      error_message = @component.state.errors ? @component.state.errors[field_key.to_sym] : nil
      has_error = !error_message.nil?

      css_class = options[:class]
      css_class = css_class.to_s if css_class
      css_class ||= ""

      if has_error
        if css_class.empty?
          css_class = @field_error_class
        else
          css_class = "#{css_class} #{@field_error_class}"
        end
      end

      attrs = {
        oninput: on_input
      }.merge(options.reject { |k, _| k == :class })

      attrs[:class] = css_class unless css_class.empty?

      @component.div do
        @component.textarea(attrs) { value }
        if has_error
          @component.div(class: @error_class) { error_message }
        end
      end
    end

    def checkbox(field_name, options = {})
      field_key = field_name.to_s
      full_key = "#{@model_key}.#{field_key}"

      value = get_nested_value(@component.state, full_key) || false

      on_change = ->(event) do
        new_value = event.target[:checked]
        set_nested_value(@model_key, field_key, new_value)
      end

      attrs = {
        type: "checkbox",
        checked: value,
        onchange: on_change
      }.merge(options)

      @component.input(attrs)
    end

    def select(field_name, choices, options = {})
      field_key = field_name.to_s
      full_key = "#{@model_key}.#{field_key}"

      value = get_nested_value(@component.state, full_key) || ""

      on_change = ->(event) do
        new_value = event.target[:value]
        set_nested_value(@model_key, field_key, new_value)
      end

      error_message = @component.state.errors ? @component.state.errors[field_key.to_sym] : nil
      has_error = !error_message.nil?

      css_class = options[:class]
      css_class = css_class.to_s if css_class
      css_class ||= ""

      if has_error
        if css_class.empty?
          css_class = @field_error_class
        else
          css_class = "#{css_class} #{@field_error_class}"
        end
      end

      attrs = {
        onchange: on_change
      }.merge(options.reject { |k, _| k == :class })

      attrs[:class] = css_class unless css_class.empty?

      @component.div do
        @component.select(attrs) do
          choices.each do |choice|
            option_value, option_text = choice.is_a?(Array) ? choice : [choice, choice]
            selected = value.to_s == option_value.to_s
            @component.option(value: option_value, selected: selected) do
              option_text
            end
          end
        end
        if has_error
          @component.div(class: @error_class) { error_message }
        end
      end
    end

    def file_field(field_name, options = {})
      field_key = field_name.to_s

      # Check if custom onchange handler is provided
      custom_handler = options.delete(:onchange)

      # Default handler: store file name in state
      default_handler = ->(event) do
        file = event.target[:files] ? event.target[:files][0] : nil
        file_name = if file.nil?
                      nil
                    else
                      # @type var file: Hash[Symbol, String]
                      file[:name]
                    end
        set_nested_value(@model_key, field_key, file_name)
      end

      on_change = custom_handler || default_handler

      attrs = {
        type: "file",
        onchange: on_change
      }.merge(options)

      @component.input(attrs)
    end

    def submit(label = "Submit", options = {})
      attrs = { type: "submit" }.merge(options)
      @component.button(attrs) { label }
    end

    def label(field_name, text = nil, options = {})
      text ||= field_name.to_s.split('_').map { |word| word.capitalize }.join(' ')
      @component.label(options) { text }
    end

    private

    # Get nested value from state (e.g., "user.username")
    def get_nested_value(state, key_path)
      keys = key_path.split('.')
      value = state
      keys.each do |key|
        if value.respond_to?(key.to_sym)
          value = value.send(key.to_sym)
        elsif value.is_a?(Hash)
          value = value[key.to_sym] || value[key]
        else
          value = nil
        end
        break if value.nil?
      end
      value
    end

    # Set nested value in state via patch
    def set_nested_value(model_key, field_key, new_value)
      # Handle nested keys like "address.city"
      if field_key.include?('.')
        # Complex nested update
        keys = field_key.split('.')
        current_model = @component.state.send(model_key.to_sym)
        updated_model = deep_merge_value(current_model, keys, new_value)
        @component.patch(model_key.to_sym => updated_model)
      else
        # Simple update
        current_model = @component.state.send(model_key.to_sym)
        if current_model.nil?
          @component.patch(model_key.to_sym => { field_key.to_sym => new_value })
        elsif current_model.is_a?(Hash)
          updated_model = current_model.merge(field_key.to_sym => new_value)
          @component.patch(model_key.to_sym => updated_model)
        else
          # Assume it's an object with instance variables
          current_model.instance_variable_set("@#{field_key}", new_value)
          @component.patch(model_key.to_sym => current_model)
        end
      end
    end

    # Deep merge helper for nested keys
    def deep_merge_value(hash, keys, value)
      return hash unless hash.is_a?(Hash)

      hash = hash.dup
      if keys.length == 1
        hash[keys[0].to_sym] = value
      else
        key = keys[0].to_sym
        hash[key] = deep_merge_value(hash[key] || {}, (keys[1..-1] || []), value)
      end
      hash
    end
  end
end
