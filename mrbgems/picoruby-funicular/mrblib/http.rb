module Funicular
  module HTTP
    class Response
      attr_reader :data, :status, :ok

      def initialize(status, data)
        @status = status
        @ok = @status >= 200 && @status < 300
        @data = data
      end

      def error?
        return true unless @ok
        return false unless @data.is_a?(Hash)
        @data["error"] || @data["errors"]
      end

      def error_message
        return nil unless @data.is_a?(Hash)
        @data["error"] || (@data["errors"].is_a?(Array) ? @data["errors"].join(", ") : @data["errors"])
      end
    end

    def self.get(url, &block)
      request("GET", url, nil, &block)
    end

    def self.post(url, body = nil, &block)
      request("POST", url, body, &block)
    end

    def self.patch(url, body = nil, &block)
      request("PATCH", url, body, &block)
    end

    def self.delete(url, &block)
      request("DELETE", url, nil, &block)
    end

    private

    def self.request(method, url, body, &block)
      options = { method: method, credentials: "include" }

      if body
        h = { "Content-Type" => "application/json" }
        # @type var h: String # steep's bug ?
        options[:headers] = h
        options[:body] = JSON.generate(body)
      end

      JS.global.fetch(url, options) do |response|
        status = response.status.to_i
        json_text = response.to_binary
        data = JSON.parse(json_text)
        # @type var status: Integer
        http_response = Response.new(status, data)
        block.call(http_response) if block
      end
    end
  end
end
