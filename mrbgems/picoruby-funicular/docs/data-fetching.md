# Data Fetching

Funicular provides multiple layers for data fetching: low-level HTTP client, high-level Model abstraction (Object-REST Mapper), and Suspense for loading states.

## Table of Contents

- [HTTP Client](#http-client)
- [Object-REST Mapper (Model)](#object-rest-mapper-model)
- [Suspense / Loading States](#suspense--loading-states)
- [Best Practices](#best-practices)

## HTTP Client

The `Funicular::HTTP` module provides a low-level interface for making HTTP requests.

### GET Requests

```ruby
Funicular::HTTP.get('/api/users') do |response|
  if response.ok
    patch(users: response.data)
  else
    patch(error: response.error_message)
  end
end
```

### POST Requests

```ruby
Funicular::HTTP.post('/api/users', { name: "Alice", email: "alice@example.com" }) do |response|
  if response.ok
    patch(user: response.data)
  else
    patch(errors: response.data["errors"])
  end
end
```

### Other HTTP Methods

```ruby
# PATCH
Funicular::HTTP.patch('/api/users/123', { name: "Alice Updated" }) do |response|
  # ...
end

# PUT
Funicular::HTTP.put('/api/users/123', user_data) do |response|
  # ...
end

# DELETE
Funicular::HTTP.delete('/api/users/123') do |response|
  # ...
end
```

### Response Object

```ruby
Funicular::HTTP.get('/api/data') do |response|
  response.status      # HTTP status code (200, 404, etc.)
  response.ok          # true if status is 2xx
  response.data        # Parsed JSON response
  response.error?      # true if response contains error
  response.error_message  # Error message from response
end
```

### CSRF Token

For non-GET requests to Rails backends, CSRF tokens are automatically included:

```ruby
# Funicular automatically:
# 1. Reads <meta name="csrf-token"> from page
# 2. Adds X-CSRF-Token header to POST/PATCH/PUT/DELETE requests
Funicular::HTTP.post('/api/posts', post_data) do |response|
  # CSRF token automatically included
end
```

## Object-REST Mapper (Model)

The Model layer provides an ActiveRecord-style interface for working with REST APIs.

### Defining Models

```ruby
class User < Funicular::Model
end

class Post < Funicular::Model
end

class Channel < Funicular::Model
end
```

### Schema Loading

Models load their attributes from a schema endpoint:

```ruby
# In your initializer
Funicular.load_schemas({
  User => "user",
  Post => "post",
  Channel => "channel"
}) do
  # Callback executed after all schemas load
  Funicular.start(container: 'app') do |router|
    # Define routes...
  end
end
```

The schema endpoint should return attribute definitions:

```ruby
# GET /api/schema/user
{
  "attributes": ["id", "username", "email", "created_at"]
}
```

### Finding Records

#### Find All

```ruby
User.all do |users, error|
  if error
    patch(error: error)
  else
    patch(users: users)
  end
end
```

#### Find by ID

```ruby
User.find(123) do |user, error|
  if error
    patch(error: "User not found")
  else
    patch(user: user)
  end
end
```

#### Find with Parameters

```ruby
Post.all({ category: "tech", limit: 10 }) do |posts, error|
  patch(posts: posts)
end
```

### Creating Records

```ruby
User.create({
  username: "alice",
  email: "alice@example.com",
  password: "secret"
}) do |user, errors|
  if errors
    # errors = { email: "Email is already taken" }
    patch(errors: errors)
  else
    patch(user: user, success: "User created!")
  end
end
```

### Updating Records

```ruby
user = state.user
user.update({ email: "newemail@example.com" }) do |updated_user, errors|
  if errors
    patch(errors: errors)
  else
    patch(user: updated_user, success: "Profile updated!")
  end
end
```

### Deleting Records

```ruby
user = state.user
user.destroy do |response, error|
  if error
    patch(error: "Failed to delete user")
  else
    patch(user: nil, success: "User deleted")
  end
end
```

### Custom Endpoints

For custom API endpoints, use the `endpoint` class method:

```ruby
class User < Funicular::Model
  def self.endpoint
    {
      "path" => "/api/v2/users",
      "find" => "/api/v2/users/:id",
      "create" => "/api/v2/users",
      "update" => "/api/v2/users/:id",
      "destroy" => "/api/v2/users/:id"
    }
  end
end
```

### Accessing Attributes

```ruby
User.find(123) do |user, error|
  user.id          # => 123
  user.username    # => "alice"
  user.email       # => "alice@example.com"
  user.created_at  # => "2024-01-15T10:30:00Z"
end
```

## Suspense / Loading States

Funicular provides a Suspense pattern for handling asynchronous data fetching with declarative loading states.

### Basic Usage

Declare async data loaders at the class level using `use_suspense`:

```ruby
class UserProfile < Funicular::Component
  use_suspense :user,
    ->(resolve, reject) {
      User.find(props[:id]) do |user, error|
        error ? reject.call(error) : resolve.call(user)
      end
    }

  def render
    div do
      h1 { "Profile" }

      suspense(
        fallback: -> { div(class: "spinner") { "Loading..." } }
      ) do
        div { "Name: #{user.name}" }
        div { "Email: #{user.email}" }
      end
    end
  end
end
```

The suspense data (`user`) is automatically accessible as a method within the component.

### With Error Handling

```ruby
suspense(
  fallback: -> { div(class: "spinner") { "Loading user..." } },
  error: ->(e) do
    div(class: "error") do
      span { "Failed to load: #{e}" }
      button(onclick: -> { reload_suspense(:user) }) { "Retry" }
    end
  end
) do
  div { "Welcome, #{user.name}" }
end
```

### Syncing with Form State

When using suspense data with forms, use `on_resolve` to sync loaded data:

```ruby
class SettingsComponent < Funicular::Component
  use_suspense :current_user,
    ->(resolve, reject) {
      Session.current_user do |user, error|
        error ? reject.call(error) : resolve.call(user)
      end
    },
    on_resolve: ->(user) {
      # Sync form state when data loads
      patch(
        form: {
          username: user.username,
          display_name: user.display_name,
          bio: user.bio
        }
      )
    }

  def initialize_state
    { form: { username: "", display_name: "", bio: "" } }
  end

  def render
    suspense(fallback: -> { div { "Loading settings..." } }) do
      form_for(:form, on_submit: :handle_save) do |f|
        f.text_field(:username, disabled: true)
        f.text_field(:display_name)
        f.textarea(:bio)
        f.submit("Save")
      end
    end
  end
end
```

### Preventing Flickering

For fast-loading data, use `min_delay` to ensure the loading state is visible for a minimum duration:

```ruby
use_suspense :data,
  ->(resolve, reject) {
    API.fetch_data do |data, error|
      error ? reject.call(error) : resolve.call(data)
    end
  },
  min_delay: 300  # Show loading spinner for at least 300ms
```

This prevents UI flickering when data loads very quickly.

### Multiple Suspense Sources

Declare multiple suspense data sources. The `suspense` helper waits for all of them:

```ruby
class Dashboard < Funicular::Component
  use_suspense :user, ->(resolve, reject) {
    User.current do |user, error|
      error ? reject.call(error) : resolve.call(user)
    end
  }

  use_suspense :stats, ->(resolve, reject) {
    Stats.fetch do |stats, error|
      error ? reject.call(error) : resolve.call(stats)
    end
  }

  use_suspense :notifications, ->(resolve, reject) {
    Notification.recent do |notifications, error|
      error ? reject.call(error) : resolve.call(notifications)
    end
  }

  def render
    suspense(fallback: -> { div { "Loading dashboard..." } }) do
      # All three are loaded before this block executes
      div do
        h1 { "Welcome, #{user.name}" }
        p { "Unread notifications: #{notifications.length}" }
        p { "Total posts: #{stats.post_count}" }
      end
    end
  end
end
```

### Reloading Data

Use `reload_suspense` to refresh data:

```ruby
def handle_refresh
  reload_suspense(:user)  # Reload specific data source
end

def render
  suspense(
    fallback: -> { div { "Loading..." } },
    error: ->(e) {
      div do
        span { "Error: #{e}" }
        button(onclick: -> { reload_suspense(:user) }) { "Retry" }
      end
    }
  ) do
    div do
      div { user.name }
      button(onclick: -> { handle_refresh }) { "Refresh" }
    end
  end
end
```

### Helper Methods

- `suspense_loading?` - Check if any suspense data is still loading
- `suspense_loading?(:name)` - Check if specific suspense data is loading
- `suspense_error?(:name)` - Check if suspense data failed to load
- `suspense_error(:name)` - Get the error for a specific suspense data
- `reload_suspense(:name)` - Reload specific suspense data

### When to Use Suspense

```ruby
# ✅ Use Suspense for: Initial data loading
use_suspense :user, ->(resolve, reject) { User.current(&resolve) }

# ❌ Don't use Suspense for: User actions (use normal state instead)
def handle_like(post_id)
  Post.like(post_id) do |result, error|
    # Just update state, don't use suspense
    patch(liked: true)
  end
end
```

## Best Practices

### 1. Use Models for CRUD Operations

```ruby
# ✅ Good: Use Model layer
User.all do |users|
  patch(users: users)
end

# ❌ Avoid: Manual HTTP for standard CRUD
Funicular::HTTP.get('/api/users') do |response|
  patch(users: response.data)
end
```

### 2. Use Suspense for Initial Loading

```ruby
# ✅ Good: Suspense for initial data
use_suspense :posts, ->(resolve, reject) {
  Post.all(&resolve)
}

# ❌ Avoid: Manual loading state management
def component_mounted
  patch(is_loading: true)
  Post.all do |posts|
    patch(posts: posts, is_loading: false)
  end
end
```

### 3. Handle Errors Gracefully

```ruby
# ✅ Good: Handle both success and error cases
User.create(form_data) do |user, errors|
  if errors
    patch(errors: errors, success_message: nil)
  else
    patch(user: user, errors: {}, success_message: "User created!")
  end
end

# ❌ Avoid: Ignoring errors
User.create(form_data) do |user, errors|
  patch(user: user)  # What if errors?
end
```

### 4. Show Loading States

```ruby
# ✅ Good: Clear loading feedback
def handle_submit(form_data)
  patch(is_saving: true, errors: {})

  Post.create(form_data) do |post, errors|
    patch(is_saving: false, errors: errors || {})
  end
end

def render
  form_for(:post, on_submit: :handle_submit) do |f|
    f.submit(
      state.is_saving ? "Saving..." : "Save",
      disabled: state.is_saving
    )
  end
end
```

### 5. Normalize Data Structures

```ruby
# ✅ Good: Normalize nested data
User.all do |users|
  users_by_id = users.each_with_object({}) { |u, h| h[u.id] = u }
  patch(users_by_id: users_by_id)
end

# ❌ Avoid: Deep nesting
patch(data: { users: { list: users, metadata: { ... } } })
```

### 6. Cleanup Async Operations

```ruby
def component_mounted
  @request_in_flight = true

  User.all do |users|
    # Only update if component is still mounted
    patch(users: users) if @request_in_flight
  end
end

def component_unmounted
  @request_in_flight = false
end
```
