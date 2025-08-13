class EvalTest < Picotest::Test
  def mruby?
    RUBY_ENGINE == "mruby"
  end

  def test_eval_simple_expression
    result = eval("1 + 1")
    assert_equal (mruby? ? 2 : nil), result
  end

  def test_eval_string
    result = eval('"hello world"')
    assert_equal (mruby? ? "hello world" : nil), result
  end

  def test_eval_variable_assignment
    eval("@test_var = 42")
    result = eval("@test_var")
    assert_equal (mruby? ? 42 : nil), result
  end

  def test_eval_method_call
    result = eval("[1,2,3].length")
    assert_equal (mruby? ? 3 : nil), result
  end

  def test_eval_syntax_error
    assert_raise SyntaxError do
      eval("1 + +")
    end
  end
end
