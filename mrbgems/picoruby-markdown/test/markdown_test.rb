class MarkdownTest < Picotest::Test
  def test_front_matter
    text = <<~MARKDOWN
      ---
      title: Test
      author: John Doe
      ---
      # Hello

      This is a test.
    MARKDOWN
    markdown = Markdown.new(text)
    assert_equal({ "title" => "Test", "author" => "John Doe" }, markdown.front_matter)
    html = markdown.to_html
    assert_true(html.include?("<h1>Hello</h1>"))
    assert_true(html.include?("<p>This is a test.</p>"))
  end

  def test_no_front_matter
    text = <<~MARKDOWN
      # Title
      Just content.
    MARKDOWN
    markdown = Markdown.new(text)
    assert_equal({}, markdown.front_matter)
    html = markdown.to_html
    assert_true(html.include?("<h1>Title</h1>"))
    assert_true(html.include?("<p>Just content.</p>"))
  end

  def test_code_block
    text = <<~MARKDOWN
      ```ruby
      def hello
        puts "hello"
      end
      ```
    MARKDOWN
    markdown = Markdown.new(text)
    expected = <<~HTML
      <pre><code class="language-ruby">def hello
        puts "hello"
      end
      </code></pre>
    HTML
    assert_equal(expected.strip, markdown.to_html.strip)
  end

  def test_html_escape_in_code_block
    text = <<~MARKDOWN
      ```
      <script>alert('XSS')</script>
      ```
    MARKDOWN
    markdown = Markdown.new(text)
    expected = <<~HTML
      <pre><code><script>alert('XSS')</script>
      </code></pre>
    HTML
    assert_equal(expected.strip, markdown.to_html.strip)
  end

  def test_table
    text = <<~MARKDOWN
      | Header 1 | Header 2 |
      |----------|----------|
      | Cell 1-1 | Cell 1-2 |
      | Cell 2-1 | Cell 2-2 |

      This is a paragraph after the table.
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html

    assert_true html.include?("<table>")
    assert_true html.include?("<thead>")
    assert_true html.include?("<th>Header 1</th>")
    assert_true html.include?("<th>Header 2</th>")
    assert_true html.include?("</thead>")
    assert_true html.include?("<tbody>")
    assert_true html.include?("<td>Cell 1-1</td>")
    assert_true html.include?("<td>Cell 1-2</td>")
    assert_true html.include?("<td>Cell 2-1</td>")
    assert_true html.include?("<td>Cell 2-2</td>")
    assert_true html.include?("</tbody>")
    assert_true html.include?("</table>")
    assert_true html.include?("<p>This is a paragraph after the table.</p>")
  end

  def test_footnote
    text = <<~MARKDOWN
      This is a sentence with a footnote.[^1]

      And another one.[^another]

      [^1]: This is the first footnote.
      [^another]: This is the second one.
    MARKDOWN

    markdown = Markdown.new(text)
    html = markdown.to_html

    # Check for footnote references in paragraphs
    assert_true html.include?(%q{<p>This is a sentence with a footnote.<sup><a href="#fn-1" id="fnref-1" rel="footnote">1</a></sup></p>})
    assert_true html.include?(%q{<p>And another one.<sup><a href="#fn-another" id="fnref-another" rel="footnote">2</a></sup></p>})

    # Check for the footnote list at the end
    assert_true html.include?(%q{<div class="footnotes">
})
    assert_true html.include?(%q{<hr>})
    assert_true html.include?(%q{<ol>})
    assert_true html.include?(%q{<li id="fn-1">This is the first footnote. <a href="#fnref-1" rev="footnote">&#8617;</a></li>})
    assert_true html.include?(%q{<li id="fn-another">This is the second one. <a href="#fnref-another" rev="footnote">&#8617;</a></li>})
    assert_true html.include?(%q{</ol>})
    assert_true html.include?(%q{</div>})
  end

  def test_unordered_list
    text = <<~MARKDOWN
      * Item 1
      * Item 2

      * Item 3
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html
    assert_true html.include?("<ul>")
    assert_true html.include?("<li>Item 1</li>")
    assert_true html.include?("<li>Item 2</li>")
    assert_true html.include?("<li>Item 3</li>")
    assert_true html.include?("</ul>")
  end

  def test_ordered_list
    text = <<~MARKDOWN
      1. First item
      2. Second item

      3. Third item
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html
    assert_true html.include?("<ol>")
    assert_true html.include?("<li>First item</li>")
    assert_true html.include?("<li>Second item</li>")
    assert_true html.include?("<li>Third item</li>")
    assert_true html.include?("</ol>")
  end

  def test_inline_formatting
    text = <<~MARKDOWN
      This is **bold** and this is __bold__ too.
      This is *italic* and this is _italic_ too.
      This is `inline code`.
      A mix of **bold** and *italic*.
      Unclosed *marker should not be formatted.
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html

    assert_true html.include?("<p>This is <strong>bold</strong> and this is <strong>bold</strong> too.</p>")
    assert_true html.include?("<p>This is <em>italic</em> and this is <em>italic</em> too.</p>")
    assert_true html.include?("<p>This is <code>inline code</code>.</p>")
    assert_true html.include?("<p>A mix of <strong>bold</strong> and <em>italic</em>.</p>")
    assert_true html.include?("<p>Unclosed *marker should not be formatted.</p>")
  end

  def test_links_and_images
    text = <<~MARKDOWN
      This is a [link](https://example.com).
      This is a ![image](/logo.png "Logo Title").
      A link with a title: [PicoRuby](https://picoruby.github.io "Go to site").
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html

    assert_true html.include?("<p>This is a <a href=\"https://example.com\">link</a>.</p>")
    assert_true html.include?("<p>This is a <img src=\"/logo.png\" alt=\"image\" title=\"Logo Title\">.</p>")
    assert_true html.include?("<p>A link with a title: <a href=\"https://picoruby.github.io\" title=\"Go to site\">PicoRuby</a>.</p>")
  end

  def test_nested_unordered_list
    text = <<~MARKDOWN
      * Item 1
        * Nested 1-1
        * Nested 1-2
      * Item 2
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html

    assert_true html.include?("<ul>")
    assert_true html.include?("<li>Item 1")
    assert_true html.include?("<li>Nested 1-1</li>")
    assert_true html.include?("<li>Nested 1-2</li>")
    assert_true html.include?("<li>Item 2</li>")
    assert_true html.include?("</ul>")
  end

  def test_nested_ordered_list
    text = <<~MARKDOWN
      1. First item
         1. Nested 1-1
         2. Nested 1-2
      2. Second item
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html

    assert_true html.include?("<ol>")
    assert_true html.include?("<li>First item")
    assert_true html.include?("<li>Nested 1-1</li>")
    assert_true html.include?("<li>Nested 1-2</li>")
    assert_true html.include?("<li>Second item</li>")
    assert_true html.include?("</ol>")
  end

  def test_mixed_nested_list
    text = <<~MARKDOWN
      * Item 1
        1. Nested ordered 1
        2. Nested ordered 2
      * Item 2
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html

    assert_true html.include?("<ul>")
    assert_true html.include?("<ol>")
    assert_true html.include?("<li>Item 1")
    assert_true html.include?("<li>Nested ordered 1</li>")
    assert_true html.include?("<li>Nested ordered 2</li>")
    assert_true html.include?("<li>Item 2</li>")
    assert_true html.include?("</ul>")
    assert_true html.include?("</ol>")
  end

  def test_deeply_nested_list
    text = <<~MARKDOWN
      * Level 1
        * Level 2
          * Level 3
      * Another Level 1
    MARKDOWN
    markdown = Markdown.new(text)
    html = markdown.to_html

    assert_true html.include?("<li>Level 1")
    assert_true html.include?("<li>Level 2")
    assert_true html.include?("<li>Level 3</li>")
    assert_true html.include?("<li>Another Level 1</li>")
  end
end
