class Session < Funicular::Model
  def self.login(username, password, &block)
    create({ username: username, password: password }, model_class: User, &block)
  end

  def self.logout(&block)
    destroy(&block)
  end

  def self.current_user(&block)
    find(endpoint_name: "current", model_class: User) do |user, error|
      block.call(user, error) if block
    end
  end
end
