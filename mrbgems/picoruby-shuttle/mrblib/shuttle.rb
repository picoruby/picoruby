require 'js'
require 'json'
require 'markdown'

class Shuttle

  # Simple YAML parser for Front Matter
  def parse_front_matter(text)
    meta = {}
    if text.start_with?("---\n")
      lines = text.lines
      lines.shift
      while (line = lines.shift) != "---\n"
        key, value = line&.split(':', 2)
        meta[key.strip] = value.strip if key && value
      end
      [meta, lines.join]
    else
      [meta, text]
    end
  end

  def is_numeric?(str)
    return false unless str.is_a?(String) && !str.empty?
    str.chars.all? { |c| c >= '0' && c <= '9' }
  end

  def initialize
    @window = JS.global
    @document = JS.document
    @content_div = @document.getElementById('content')
  end

  def parse_query_string(query_string)
    params = {}
    return params if query_string.nil? || query_string.empty?

    query_string.split('&').each do |pair|
      key, value = pair.split('=', 2)
      params[key] = value if key
    end
    params
  end

  def render
    full_url = @window.location&.href&.to_s

    query_part = ""
    if (query_index = full_url.index('?'))
      query_part = full_url.slice(query_index + 1 .. -1) || ""
      # Remove hash fragment if present
      if (hash_index = query_part.index('#'))
        query_part = query_part.slice(0 .. hash_index - 1)
      end
    end

    params = parse_query_string(query_part)

    if params['article']
      render_article(params['article'])
    elsif params['page']
      page = is_numeric?(params['page']) ? params['page'].to_i : 1
      render_index(page)
    else
      render_index(1)
    end
  end

  def render_index(page = 1)
    @window.fetch("./articles/index_#{page}.json") do |response|
      # Following the `funicular` pattern, get the response body as a string and parse it as JSON
      json_text = response.to_binary
      begin
        data = JSON.parse(json_text)
        html = "<ul>"

        # Access the key from the parsed Ruby Hash
        articles = data['articles']

        if articles.is_a?(Array)
          articles.each do |article|
            puts "Article ID: #{article['id'].inspect}, Class: #{article['id'].class}"
            # `article` is now a Ruby Hash
            html += "<li><a href='?article=#{article['id']}'>#{article['title']}</a></li>"
          end
        end
        html += "</ul>"
        @content_div.innerHTML = html
      rescue JSON::ParserError => e
        @content_div.innerHTML = "<p>Failed to parse index.json: #{e.message}</p>"
      end
    end
  end

  def render_article(article_id)
    @window.fetch("./articles/#{article_id}.md") do |response|
      markdown_text = response.to_binary
      meta, content = parse_front_matter(markdown_text)
      title = meta['title'] || article_id.gsub('_', ' ').capitalize
      html = "<h1>#{title}</h1>"
      html += Markdown.new(content).to_html
      @content_div.innerHTML = html
    end
  end

  def self.run
    shuttle = Shuttle.new
    shuttle.render

    # Handle browser back/forward buttons
    JS.global.addEventListener('popstate') do
      shuttle.render
    end

    # Intercept link clicks to prevent page reload and use History API
    JS.document.addEventListener('click') do |event|
      target = event&.target
      # Check if clicked element is a link
      if target && target.tagName&.to_s&.upcase == 'A'
        href = target.getAttribute('href')&.to_s
        # Only handle query string links (not external or hash links)
        if href && href.start_with?('?')
          event.preventDefault
          # Update URL without page reload
          JS.global.history&.pushState(JS::Null, '', href)
          # Render new content
          shuttle.render
        end
      end
    end
  end
end
