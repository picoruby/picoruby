puts "Funicular Chat App initializing..."

# Mount JavaScript helpers
Funicular::FileUpload.mount

# Load all model schemas before starting the app
Funicular.load_schemas({ User => "user", Session => "session", Channel => "channel" }) do
  # Start the application after all schemas are loaded
  Funicular.start(container: 'app') do |router|
    router.get('/login', to: LoginComponent, as: 'login')
    router.get('/chat/:channel_id', to: ChatComponent, as: 'chat_channel')
    router.get('/chat', to: ChatComponent, as: 'chat')
    router.get('/settings', to: SettingsComponent, as: 'settings')
    router.set_default('/login')
  end
end
