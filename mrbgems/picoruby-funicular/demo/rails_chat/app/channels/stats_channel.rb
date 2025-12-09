class StatsChannel < ApplicationCable::Channel
  def subscribed
    channel = Channel.find(params[:channel_id])
    stream_from "stats_#{channel.id}"

    # Send initial stats immediately
    send_initial_stats(channel)
  rescue ActiveRecord::RecordNotFound
    reject
  end

  def unsubscribed
    stop_all_streams
  end

  private

  def send_initial_stats(channel)
    messages = channel.messages
      .where('created_at > ?', 1.year.ago)
      .group(:user_id)
      .count

    stats = User.where(id: messages.keys).order(:display_name).map do |user|
      {
        user_id: user.id,
        username: user.display_name,
        count: messages[user.id]
      }
    end

    transmit({
      type: "stats_update",
      stats: stats
    })
  end
end
