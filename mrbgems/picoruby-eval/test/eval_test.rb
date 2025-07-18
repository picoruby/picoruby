class EvalTest < Picotest::Test
  def test_eval_simple_expression
    result = eval("1 + 1")
    assert_equal 2, result
  end

  def test_eval_string
    result = eval('"hello world"')
    assert_equal "hello world", result
  end

  def test_eval_variable_assignment
    eval("@test_var = 42")
    result = eval("@test_var")
    assert_equal 42, result
  end

  def test_eval_method_call
    result = eval("[1,2,3].length")
    assert_equal 3, result
  end

  def test_eval_syntax_error
    assert_raise SyntaxError do
      eval("1 + +")
    end
  end
end
