class ShellTokenizerTest < Picotest::Test
  def tokens(input)
    t = Shell::Tokenizer.new(input)
    out = []
    while (tok = t.next_token)
      out << [tok[:type], input[tok[:pos], tok[:length]]]
    end
    out
  end

  def test_words_are_split_on_whitespace
    assert_equal([[:word, "ls"], [:word, "-la"], [:word, "/home"]], tokens("ls -la /home"))
    assert_equal([[:word, "a"], [:word, "b"]], tokens("  a \t b \n"))
  end

  def test_empty_input_has_no_token
    assert_nil(Shell::Tokenizer.new("").next_token)
    assert_nil(Shell::Tokenizer.new("   ").next_token)
  end

  def test_single_specials
    assert_equal([[:word, "a"], [:special, "|"], [:word, "b"]], tokens("a|b"))
    assert_equal([[:word, "a"], [:special, "&"], [:word, "b"]], tokens("a & b"))
    assert_equal([[:word, "a"], [:special, ";"], [:word, "b"]], tokens("a;b"))
    assert_equal([[:word, "a"], [:special, ">"], [:word, "b"]], tokens("a>b"))
    assert_equal([[:word, "a"], [:special, "<"], [:word, "b"]], tokens("a < b"))
  end

  def test_doubled_redirections_are_one_token
    assert_equal([[:word, "a"], [:special, ">>"], [:word, "b"]], tokens("a >> b"))
    assert_equal([[:word, "a"], [:special, "<<"], [:word, "b"]], tokens("a << b"))
    # a third sign starts a new token
    assert_equal([[:word, "a"], [:special, ">>"], [:special, ">"], [:word, "b"]], tokens("a >>> b"))
  end

  def test_quoted_strings_keep_their_quotes
    assert_equal([[:word, "echo"], [:quoted_string, "\"hello world\""]], tokens("echo \"hello world\""))
    assert_equal([[:word, "echo"], [:quoted_string, "'a|b'"]], tokens("echo 'a|b'"))
  end

  def test_unterminated_quote_runs_to_the_end
    assert_equal([[:word, "echo"], [:quoted_string, "\"abc"]], tokens("echo \"abc"))
    assert_equal([[:quoted_string, "\""]], tokens("\""))
  end

  def test_token_positions
    t = Shell::Tokenizer.new("ls  -la")
    first = t.next_token
    second = t.next_token
    assert_equal(0, first[:pos])
    assert_equal(2, first[:length])
    assert_equal(4, second[:pos])
    assert_equal(3, second[:length])
  end
end

class ShellParserTest < Picotest::Test
  def parse(input)
    Shell::Parser.new(input).parse
  end

  def test_empty_input_parses_to_nil
    assert_nil(parse(""))
    assert_nil(parse("   "))
  end

  def test_single_command
    node = parse("ls")
    assert_equal(:command, node.type)
    assert_equal("ls", node.data[:name])
    assert_equal([], node.data[:args])
    assert_equal([], node.data[:redirects])
    assert_equal("ls", node.token)
  end

  def test_command_with_arguments
    node = parse("ls -la /home")
    assert_equal("ls", node.data[:name])
    assert_equal(["-la", "/home"], node.data[:args])
    assert_equal("ls -la /home", node.token)
  end

  def test_quoted_argument_loses_its_quotes
    node = parse("echo \"hello world\" 'x y'")
    assert_equal(["hello world", "x y"], node.data[:args])
  end

  def test_quoted_command_name
    node = parse("\"my cmd\" arg")
    assert_equal("my cmd", node.data[:name])
    assert_equal(["arg"], node.data[:args])
  end

  def test_pipeline
    node = parse("ls | grep foo")
    assert_equal(:pipeline, node.type)
    commands = node.data[:commands]
    assert_equal(2, commands.size)
    assert_equal("ls", commands[0].data[:name])
    assert_equal("grep", commands[1].data[:name])
    assert_equal(["foo"], commands[1].data[:args])
    assert_equal("ls | grep foo", node.token)
  end

  def test_three_stage_pipeline
    node = parse("a|b|c")
    assert_equal(:pipeline, node.type)
    assert_equal(["a", "b", "c"], node.data[:commands].map { |c| c.data[:name] })
  end

  def test_output_and_input_redirections
    node = parse("cat < in.txt > out.txt")
    assert_equal(:command, node.type)
    assert_equal("cat", node.data[:name])
    assert_equal([], node.data[:args])
    redirects = node.data[:redirects]
    assert_equal(2, redirects.size)
    assert_equal(:redirection, redirects[0].type)
    assert_equal(:input, redirects[0].data[:type])
    assert_equal("in.txt", redirects[0].data[:target])
    assert_equal("< in.txt", redirects[0].token)
    assert_equal(:output, redirects[1].data[:type])
    assert_equal("out.txt", redirects[1].data[:target])
    assert_equal("> out.txt", redirects[1].token)
  end

  def test_append_and_here_document
    node = parse("echo hi >> log.txt")
    assert_equal(["hi"], node.data[:args])
    assert_equal(:append, node.data[:redirects][0].data[:type])
    assert_equal("log.txt", node.data[:redirects][0].data[:target])
    node = parse("cat << EOF")
    assert_equal(:here_document, node.data[:redirects][0].data[:type])
    assert_equal("EOF", node.data[:redirects][0].data[:target])
  end

  def test_quoted_redirection_target
    node = parse("echo x > \"my file.txt\"")
    assert_equal("my file.txt", node.data[:redirects][0].data[:target])
  end

  def test_redirection_inside_a_pipeline
    node = parse("ls -la /home | grep foo > out.txt")
    assert_equal(:pipeline, node.type)
    first, second = node.data[:commands]
    assert_equal(["-la", "/home"], first.data[:args])
    assert_equal([], first.data[:redirects])
    assert_equal(["foo"], second.data[:args])
    assert_equal(:output, second.data[:redirects][0].data[:type])
    assert_equal("out.txt", second.data[:redirects][0].data[:target])
  end

  def test_parsing_stops_at_an_unknown_special
    node = parse("ls;echo hi")
    assert_equal(:command, node.type)
    assert_equal("ls", node.data[:name])
    assert_equal("ls", node.token)
    node = parse("sleep 3 &")
    assert_equal(["3"], node.data[:args])
  end

  def test_a_pipe_without_a_command_before_it_raises
    assert_raise(RuntimeError) { parse("| ls") }
  end

  def test_a_trailing_pipe_is_ignored
    node = parse("ls |")
    assert_equal(:command, node.type)
    assert_equal("ls", node.data[:name])
  end

  def test_a_redirection_without_a_target_raises
    assert_raise(RuntimeError) { parse("ls >") }
    assert_raise(RuntimeError) { parse("ls > | wc") }
  end
end

class ShellPipeIOTest < Picotest::Test
  def test_puts_queues_whole_lines
    io = Shell::Pipeline::PipeIO.new
    io.puts "one"
    io.puts "two"
    assert_equal("one\n", io.gets)
    assert_equal("two\n", io.gets)
    assert_nil(io.gets)
  end

  def test_print_keeps_a_partial_line_until_read
    io = Shell::Pipeline::PipeIO.new
    io.print "a\nb"
    assert_false(io.empty?)
    assert_equal("a\n", io.gets)
    assert_equal("b", io.gets)
    assert_nil(io.gets)
    assert(io.empty?)
  end

  def test_a_partial_line_joins_the_next_newline
    io = Shell::Pipeline::PipeIO.new
    io.print "ab"
    io.print "c\nd\n"
    assert_equal("abc\n", io.gets)
    assert_equal("d\n", io.gets)
    assert_nil(io.gets)
  end

  def test_write_answers_the_size_and_each_line_drains
    io = Shell::Pipeline::PipeIO.new
    assert_equal(4, io.write("x\ny\n"))
    lines = []
    io.each_line { |l| lines << l }
    assert_equal(["x\n", "y\n"], lines)
    assert(io.empty?)
  end

  def test_clear_drops_everything
    io = Shell::Pipeline::PipeIO.new
    io.puts "a"
    io.print "b"
    io.clear
    assert(io.empty?)
    assert_nil(io.gets)
  end

  def test_puts_with_no_argument_is_an_empty_line
    io = Shell::Pipeline::PipeIO.new
    io.puts
    assert_equal("\n", io.gets)
  end
end

class ShellPipelineParseTest < Picotest::Test
  def test_split_on_pipes_and_whitespace
    assert_equal([["ls", "-la"], ["grep", "foo"]], Shell::Pipeline.parse("ls -la | grep foo"))
    assert_equal([["a"], ["b"], ["c"]], Shell::Pipeline.parse("a|b|c"))
  end

  def test_empty_segments_are_dropped
    assert_equal([], Shell::Pipeline.parse(""))
    assert_equal([], Shell::Pipeline.parse(" | "))
    assert_equal([["ls"]], Shell::Pipeline.parse("ls |"))
  end
end

class ShellJobTest < Picotest::Test
  def test_a_job_needs_a_command
    assert_raise(ArgumentError) { Shell::Job.new }
  end
end
