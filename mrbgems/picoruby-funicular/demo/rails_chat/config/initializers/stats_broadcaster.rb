Rails.application.config.after_initialize do
  Thread.new do
    loop do
      sleep 15
      begin
        BroadcastStatsJob.perform_now
      rescue => e
        Rails.logger.error "Stats broadcast error: #{e.message}"
      end
    end
  rescue => e
    Rails.logger.error "Stats broadcaster thread crashed: #{e.message}"
  end
end
