class BroadcastStatsJob < ApplicationJob
  queue_as :default

  def perform
    Channel.find_each do |channel|
      stats = calculate_stats(channel)

      ActionCable.server.broadcast "stats_#{channel.id}", {
        type: "stats_update",
        stats: stats
      }
    end
  end

  private

  def calculate_stats(channel)
    messages = channel.messages
      .where('created_at > ?', 1.year.ago)
      .group(:user_id)
      .count

    User.where(id: messages.keys).order(:display_name).map do |user|
      {
        user_id: user.id,
        username: user.display_name,
        count: messages[user.id]
      }
    end
  end
end
