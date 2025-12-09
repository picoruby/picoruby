class ChatChannel < ApplicationCable::Channel
  def subscribed
    channel = Channel.find(params[:channel_id])
    stream_from "chat_#{channel.id}"

    messages = channel.messages.includes(:user).order(created_at: :desc).limit(100).reverse

    transmit({
      type: "initial_messages",
      messages: messages.map { |m|
        {
          id: m.id,
          content: m.content,
          created_at: m.created_at.iso8601,
          user: {
            id: m.user.id,
            username: m.user.username,
            display_name: m.user.display_name,
            has_avatar: m.user.avatar.present?
          }
        }
      }
    })
  rescue ActiveRecord::RecordNotFound
    reject
  end

  def unsubscribed
    stop_all_streams
  end

  def send_message(data)
    channel = Channel.find(params[:channel_id])
    message = channel.messages.create!(
      user: current_user,
      content: data["content"]
    )

    ActionCable.server.broadcast "chat_#{channel.id}", {
      type: "new_message",
      message: {
        id: message.id,
        content: message.content,
        created_at: message.created_at.iso8601,
        user: {
          id: current_user.id,
          username: current_user.username,
          display_name: current_user.display_name,
          has_avatar: current_user.avatar.present?
        }
      }
    }
  rescue ActiveRecord::RecordInvalid => e
    transmit({ type: "error", message: e.message })
  end

end
