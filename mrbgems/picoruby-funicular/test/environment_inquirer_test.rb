class EnvironmentInquirerTest < Picotest::Test
  def setup
    Funicular.env = nil
    ENV.delete 'RAILS_ENV'
    ENV.delete 'FUNICULAR_ENV'
  end

  def test_default_environment
    assert_equal('development', Funicular.env.to_s)
    assert_equal(true, Funicular.env.development?)
    assert_equal(false, Funicular.env.test?)
  end

  def test_RAILS_ENV_works
    ENV['RAILS_ENV'] = 'production'
    assert_equal('production', Funicular.env.to_s)
  end

  def test_FUNICULAR_ENV_works
    ENV['RAILS_ENV'] = 'production'
    ENV['FUNICULAR_ENV'] = 'test'
    assert_equal('test', Funicular.env.to_s)
  end
end

