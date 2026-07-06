class EvalTest < Picotest::Test
  def picoruby?
    RUBY_ENGINE == "mruby"
  end

  def test_eval_simple_expression
    result = eval("1 + 1")
    assert_equal (picoruby? ? 2 : nil), result
  end

  def test_eval_string
    result = eval('"hello world"')
    assert_equal (picoruby? ? "hello world" : nil), result
  end

  def test_eval_variable_assignment
    eval("@test_var = 42")
    result = eval("@test_var")
    assert_equal (picoruby? ? 42 : nil), result
  end

  def test_eval_method_call
    result = eval("[1,2,3].length")
    assert_equal (picoruby? ? 3 : nil), result
  end

  def test_eval_syntax_error
    assert_raise SyntaxError do
      eval("1 + +")
    end
  end
end
