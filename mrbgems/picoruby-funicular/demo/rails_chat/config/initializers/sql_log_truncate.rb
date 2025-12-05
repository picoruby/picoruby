# Truncate binary data in SQL logs to prevent excessive log output
if Rails.env.development?
  ActiveSupport::LogSubscriber.log_subscribers.each do |subscriber|
    if subscriber.is_a?(ActiveRecord::LogSubscriber)
      class << subscriber
        prepend(Module.new do
          def sql(event)
            # Truncate long binary data in SQL statements
            event.payload[:sql] = event.payload[:sql].gsub(/x'[0-9a-f]{100,}'/) do |match|
              "x'#{match[2..101]}...<truncated #{match.length - 2} hex chars>'"
            end
            super(event)
          end
        end)
      end
    end
  end
end
