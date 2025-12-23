# picoruby-markdown

A pure Ruby Markdown parser designed for PicoRuby environment. It focuses on memory efficiency and avoids complex parsing techniques like Abstract Syntax Trees (ASTs) or regular expressions.

## Features

- **Pure Ruby Implementation**: No C extensions, making it suitable for resource-constrained environments.
- **HTML Generation**: Converts Markdown text into HTML.
- **Memory Efficient**: Parses Markdown without generating an Abstract Syntax Tree (AST).
- **No Regular Expressions**: Parsing is done using basic string operations, adhering to the project's constraints.
- **Front Matter Support**: Integrates with `picoruby-yaml` to parse YAML front matter.

### Supported Markdown Syntaxes

This gem supports a subset of Markdown features, sufficient for typical blog post content:

-   **Headers**: `# Heading 1`, `## Heading 2`, etc.
-   **Code Blocks**: Fenced code blocks using triple backticks (```).
-   **Tables**: Basic GitHub Flavored Markdown (GFM) tables.
    ```markdown
    | Header 1 | Header 2 |
    |----------|----------|
    | Cell 1-1 | Cell 1-2 |
    ```
-   **Footnotes**: `Some text[^1]`, and `[^1]: Footnote content.`
-   **Lists**: Unordered (`*`, `-`, `+`) and Ordered (`1.`) lists.
-   **Bold**: `**bold**` or `__bold__`
-   **Italic**: `*italic*` or `_italic_`
-   **Inline Code**: `` `inline code` ``
-   **Links**: `[text](url "optional title")`
-   **Images**: `![alt text](url "optional title")`

## Installation

To use `picoruby-markdown` in your PicoRuby project, add it as a gem dependency in your `mrbgem.rake` or `build_config.rb`:

```ruby
# In your mrbgem.rake or build_config.rb
MRuby::Gem::Specification.new('picoruby-markdown') do |spec|
  spec.license = 'MIT'
  spec.author  = 'hasumikin'
  spec.add_dependency 'picoruby-yaml'
  spec.add_dependency 'picoruby-picotest' # For development/testing
end
```

Then, rebuild your PicoRuby firmware or application.

## Usage

```ruby
require 'markdown'

markdown_text = <<~MARKDOWN
  ---
  title: My First Blog Post
  author: John Doe
  date: 2025-12-23
  ---

  # Welcome to PicoRuby Markdown!

  This is a paragraph with **bold** and *italic* text.

  ```ruby
  def hello_picoruby
    puts "Hello, PicoRuby!"
  end
  ```

  Here's a list:
  * Item 1
  * Item 2

  And a table:
  | Feature    | Supported |
  |------------|-----------|
  | Tables     | Yes       |
  | Footnotes  | Yes       |

  Footnotes are easy.[^footnote_example]

  [^footnote_example]: This is an example footnote.

  Check out [PicoRuby official site](https://picoruby.github.io "PicoRuby")!
  ![PicoRuby Logo](/assets/picoruby-logo.png "The PicoRuby Logo")
MARKDOWN

markdown_parser = Markdown.new(markdown_text)

# Access Front Matter
puts "Title: #{markdown_parser.front_matter['title']}"
puts "Author: #{markdown_parser.front_matter['author']}"

# Convert to HTML
html_output = markdown_parser.to_html
puts html_output
```

## Limitations

-   **Nesting**: Nested inline elements (e.g., `**bold *italic* in bold**`) are not fully supported and might not render as expected.
-   **Complex Cases**: Some advanced Markdown syntaxes (e.g., nested lists, blockquotes, definition lists, block-level HTML) are not supported.
-   **Alignment**: Table column alignment (e.g., `:--`, `:-:`, `--:`) is parsed but not currently applied to the generated HTML.
-   **Performance**: While memory-efficient, the parsing logic is primarily string-based and might not be as fast as C-accelerated or regex-heavy parsers for very large documents.

## Contributing

Bug reports and pull requests are welcome on GitHub.

## License

The gem is available as open source under the terms of the [MIT License](https://opensource.org/licenses/MIT).
