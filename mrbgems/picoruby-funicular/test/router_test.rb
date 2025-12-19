class RouterTest < Picotest::Test
  class MyComponent
  end

  def setup
    @container = stub(Object.new) # JS::Object
    @router = Funicular::Router.new(@container)
  end

  # Test route registration
  def test_add_route
    @router.get('/posts', to: MyComponent, as: "posts")
    route = @router.routes.first
    assert_equal('/posts', route[:path])
    assert_equal(MyComponent, route[:component])
    assert_equal(:get, route[:method])
    url_helpers = @router.instance_variable_get(:@url_helpers)
    assert_equal(Module, url_helpers.class)
    assert_equal(true, url_helpers.method_defined?(:posts_path))
  end

  # Test POST method
  def test_post_route
    @router.post('/posts', to: MyComponent, as: "create_post")
    route = @router.routes.first
    assert_equal('/posts', route[:path])
    assert_equal(MyComponent, route[:component])
    assert_equal(:post, route[:method])
    url_helpers = @router.instance_variable_get(:@url_helpers)
    assert_equal(true, url_helpers.method_defined?(:create_post_path))
  end

  # Test PUT method
  def test_put_route
    @router.put('/posts/:id', to: MyComponent, as: "update_post")
    route = @router.routes.first
    assert_equal('/posts/:id', route[:path])
    assert_equal(MyComponent, route[:component])
    assert_equal(:put, route[:method])
    url_helpers = @router.instance_variable_get(:@url_helpers)
    assert_equal(true, url_helpers.method_defined?(:update_post_path))
  end

  # Test PATCH method
  def test_patch_route
    @router.patch('/posts/:id', to: MyComponent, as: "patch_post")
    route = @router.routes.first
    assert_equal('/posts/:id', route[:path])
    assert_equal(MyComponent, route[:component])
    assert_equal(:patch, route[:method])
    url_helpers = @router.instance_variable_get(:@url_helpers)
    assert_equal(true, url_helpers.method_defined?(:patch_post_path))
  end

  # Test DELETE method
  def test_delete_route
    @router.delete('/posts/:id', to: MyComponent, as: "delete_post")
    route = @router.routes.first
    assert_equal('/posts/:id', route[:path])
    assert_equal(MyComponent, route[:component])
    assert_equal(:delete, route[:method])
    url_helpers = @router.instance_variable_get(:@url_helpers)
    assert_equal(true, url_helpers.method_defined?(:delete_post_path))
  end

  # Test URL helper without parameters
  def test_url_helper_without_params
    @router.get('/posts', to: MyComponent, as: "posts")
    url_helpers = @router.instance_variable_get(:@url_helpers)
    helper = Object.new
    helper.extend(url_helpers)
    assert_equal('/posts', helper.posts_path)
  end

  # Test URL helper with single parameter
  def test_url_helper_with_single_param
    @router.get('/posts/:id', to: MyComponent, as: "post")
    url_helpers = @router.instance_variable_get(:@url_helpers)
    helper = Object.new
    helper.extend(url_helpers)
    assert_equal('/posts/123', helper.post_path(123))
  end

  # Test URL helper with multiple parameters
  def test_url_helper_with_multiple_params
    @router.get('/users/:user_id/posts/:id', to: MyComponent, as: "user_post")
    url_helpers = @router.instance_variable_get(:@url_helpers)
    helper = Object.new
    helper.extend(url_helpers)
    assert_equal('/users/42/posts/123', helper.user_post_path(42, 123))
  end

  # Test find_route with exact match
  def test_find_route_exact_match
    @router.get('/posts', to: MyComponent)
    component_class, params = @router.send(:find_route, '/posts')
    assert_equal(MyComponent, component_class)
    assert_equal({}, params)
  end

  # Test find_route with parameters
  def test_find_route_with_params
    @router.get('/posts/:id', to: MyComponent)
    component_class, params = @router.send(:find_route, '/posts/123')
    assert_equal(MyComponent, component_class)
    assert_equal({id: '123'}, params)
  end

  # Test find_route with multiple parameters
  def test_find_route_with_multiple_params
    @router.get('/users/:user_id/posts/:id', to: MyComponent)
    component_class, params = @router.send(:find_route, '/users/42/posts/123')
    assert_equal(MyComponent, component_class)
    assert_equal({user_id: '42', id: '123'}, params)
  end

  # Test find_route with no match
  def test_find_route_no_match
    @router.get('/posts', to: MyComponent)
    component_class, params = @router.send(:find_route, '/users')
    assert_equal(nil, component_class)
  end

  # Test set_default method
  def test_set_default
    @router.set_default('/home')
    default_route = @router.instance_variable_get(:@default_route)
    assert_equal('/home', default_route)
  end

  # Test add_route method (backward compatibility)
  def test_add_route_method
    @router.add_route('/about', MyComponent, as: "about")
    route = @router.routes.first
    assert_equal('/about', route[:path])
    assert_equal(MyComponent, route[:component])
    assert_equal(:get, route[:method])
    url_helpers = @router.instance_variable_get(:@url_helpers)
    assert_equal(true, url_helpers.method_defined?(:about_path))
  end

  # Test multiple routes registration
  def test_multiple_routes
    @router.get('/posts', to: MyComponent, as: "posts")
    @router.post('/posts', to: MyComponent, as: "create_post")
    @router.get('/posts/:id', to: MyComponent, as: "post")
    assert_equal(3, @router.routes.length)
    assert_equal(:get, @router.routes[0][:method])
    assert_equal(:post, @router.routes[1][:method])
    assert_equal(:get, @router.routes[2][:method])
  end

end
