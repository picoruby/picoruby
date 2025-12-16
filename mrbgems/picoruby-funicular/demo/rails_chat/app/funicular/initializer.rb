puts "Funicular Chat App initializing..."

# Mount JavaScript helpers
Funicular::FileUpload.mount

# Load all model schemas before starting the app
Funicular.load_schemas({ User => "user", Session => "session", Channel => "channel" }) do
  # Start the application after all schemas are loaded
  Funicular.start(container: 'app') do |router|
    router.add_route('/login', LoginComponent)
    router.add_route('/chat/:channel_id', ChatComponent)
    router.add_route('/chat', ChatComponent)
    router.add_route('/settings', SettingsComponent)
    router.set_default('/login')
  end
end
