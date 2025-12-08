class SessionsController < ApplicationController
  skip_before_action :verify_authenticity_token, only: [:create, :destroy]

  def create
    user = User.find_by(username: params[:username])

    if user&.authenticate(params[:password])
      session[:user_id] = user.id
      render json: {
        id: user.id,
        username: user.username,
        display_name: user.display_name,
        has_avatar: user.avatar.present?
      }
    else
      render json: { error: "Invalid username or password" }, status: :unauthorized
    end
  end

  def show
    if logged_in?
      render json: {
        id: current_user.id,
        username: current_user.username,
        display_name: current_user.display_name,
        has_avatar: current_user.avatar.present?
      }
    else
      render json: { error: "Not logged in" }, status: :unauthorized
    end
  end

  def destroy
    session[:user_id] = nil
    render json: { message: "Logged out successfully" }
  end
end
