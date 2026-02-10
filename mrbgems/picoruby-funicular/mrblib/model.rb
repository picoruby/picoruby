module Funicular
  class Model
    attr_reader :id

    class << self
      attr_accessor :schema, :endpoints
    end

    def self.load_schema(schema_data)
      @schema = schema_data["attributes"]
      @endpoints = schema_data["endpoints"]

      # Generate attr_accessor dynamically based on schema
      @schema.each do |name, config|
        attr_reader name.to_sym

        unless config["readonly"]
          define_method("#{name}=") do |value|
            instance_variable_set("@#{name}", value)
            @changed_attributes ||= {}
            @changed_attributes[name] = value
          end
        end
      end
    end

    def initialize(attributes = {})
      @changed_attributes = {}
      # Set attributes based on schema
      self.class.schema.each do |name, config|
        value = attributes[name] || attributes[name.to_sym]
        instance_variable_set("@#{name}", value)
      end
    end

    def self.all(params = {}, &block)
      endpoint = @endpoints["all"]
      return unless endpoint

      HTTP.get(endpoint["path"]) do |response|
        if response.error?
          block.call(nil, response.error_message) if block
        else
          instances = response.data.map { |attrs| new(attrs) }
          block.call(instances, nil) if block
        end
      end
    end

    def self.find(id = nil, endpoint_name: "find", model_class: nil, &block)
      endpoint = @endpoints[endpoint_name]
      return unless endpoint

      path = endpoint["path"]
      path = path.gsub(":id", id.to_s) if id

      HTTP.get(path) do |response|
        if response.error?
          block.call(nil, response.error_message) if block
        else
          klass = model_class || self
          instance = klass.new(response.data)
          block.call(instance, nil) if block
        end
      end
    end

    def self.create(attrs, model_class: nil, &block)
      endpoint = @endpoints["create"]
      return unless endpoint

      HTTP.post(endpoint["path"], attrs) do |response|
        if response.error?
          block.call(nil, response.error_message) if block
        else
          klass = model_class || self
          instance = klass.new(response.data)
          block.call(instance, nil) if block
        end
      end
    end

    def self.destroy(id = nil, &block)
      endpoint = @endpoints["destroy"]
      return unless endpoint

      path = id ? endpoint["path"].gsub(":id", id.to_s) : endpoint["path"]

      HTTP.delete(path) do |response|
        if response.error?
          block.call(false, response.error_message) if block
        else
          block.call(true, response.data) if block
        end
      end
    end

    def update(attrs = nil, &block)
      if attrs
        attrs.each { |k, v| send("#{k}=", v) }
      end

      return if @changed_attributes.empty?

      json_attrs = @changed_attributes.reject do |name, value|
        schema = self.class.schema[name]
        schema && schema["type"] == "binary"
      end

      return if json_attrs.empty?

      endpoint = self.class.endpoints["update"]
      path = endpoint["path"].gsub(":id", @id.to_s)

      HTTP.patch(path, json_attrs) do |response|
        if response.error?
          block.call(false, response.error_message) if block
        else
          # Update attributes with response data
          response.data.each do |key, value|
            instance_variable_set("@#{key}", value)
          end
          @changed_attributes = {}
          block.call(true, response.data) if block
        end
      end
    end

    def destroy(&block)
      self.class.destroy(@id, &block)
    end

    def reload(&block)
      self.class.find(@id) do |instance, error|
        if instance
          instance.instance_variables.each do |var|
            instance_variable_set(var, instance.instance_variable_get(var))
          end
          @changed_attributes = {}
        end
        block.call(instance, error) if block
      end
    end
  end
end
