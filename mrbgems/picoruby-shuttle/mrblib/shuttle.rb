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

  def render_404(title = "Page Not Found", message = "The page you are looking for does not exist.")
    html = "<div class='error-page'>"
    html += "<div class='error-content'>"
    html += "<h1 class='error-code'>404</h1>"
    html += "<h2 class='error-title'>#{title}</h2>"
    html += "<p class='error-message'>#{message}</p>"
    html += "<div class='error-actions'>"
    html += "<a href='./' class='error-link'>Go to Home</a>"
    html += "</div>"
    html += "</div>"
    html += "</div>"
    @content_div.innerHTML = html
  end

  def render_index(page = 1)
    @window.fetch("./generated/index_#{page}.json") do |response|
      status = response.status.to_i
      if status == 404
        render_404("Page not found", "The page you are looking for does not exist.")
      elsif status >= 200 && status < 300
        json_text = response.to_binary
        begin
          data = JSON.parse(json_text)
          html = "<div class='article-list'>"
          html += "<h2 class='article-list-title'>Recent Articles</h2>"

          total_pages = data['total_pages'] || 1
          current_page = data['page'] || 1
          total_articles = data['total_articles'] || 0

          html += "<div class='article-list-items'>"

          articles = data['articles']

          if articles.is_a?(Array)
            articles.each do |article|
              # puts "Article ID: #{article['id'].inspect}, Class: #{article['id'].class}"
              html += "<article class='article-card group'>"
              html += "<h3 class='article-card-title'>"
              html += "<a href='?article=#{article['id']}' class='article-card-link'>#{article['title']}</a>"
              html += "</h3>"
              html += "<p class='article-card-date'>#{article['date']}</p>"
              html += "</article>"
            end
          end
          html += "</div>"

          # Pagination
          if total_pages > 1
            html += "<nav class='pagination'>"
            html += "<div class='pagination-info'>"
            html += "Page #{current_page} of #{total_pages} (#{total_articles} articles total)"
            html += "</div>"
            html += "<div class='pagination-links'>"

            # Previous button
            if current_page > 1
              html += "<a href='?page=#{current_page - 1}' class='pagination-link'>Previous</a>"
            else
              html += "<span class='pagination-link pagination-disabled'>Previous</span>"
            end

            # Page numbers
            (1..total_pages).each do |p|
              if p == current_page
                html += "<span class='pagination-link pagination-current'>#{p}</span>"
              else
                html += "<a href='?page=#{p}' class='pagination-link'>#{p}</a>"
              end
            end

            # Next button
            if current_page < total_pages
              html += "<a href='?page=#{current_page + 1}' class='pagination-link'>Next</a>"
            else
              html += "<span class='pagination-link pagination-disabled'>Next</span>"
            end

            html += "</div>"
            html += "</nav>"
          end

          html += "</div>"
          @content_div.innerHTML = html
          highlight_code
        rescue JSON::ParserError => e
          @content_div.innerHTML = "<p>Failed to parse index.json: #{e.message}</p>"
        end
      else
        render_404("Error loading page", "An error occurred while loading the page (HTTP #{status}).")
      end
    end
  end

  def render_article(article_id)
    @window.fetch("./articles/#{article_id}.md") do |response|
      status = response.status.to_i
      if status == 404
        render_404("Article not found", "The article you are looking for does not exist.")
      elsif status >= 200 && status < 300
        markdown_text = response.to_binary
        meta, content = parse_front_matter(markdown_text)
        title = meta['title'] || article_id.gsub('_', ' ').capitalize
        date = meta['date'] || ''

        html = "<article>"
        html += "<div class='article-header'>"
        html += "<h1 class='article-title'>#{title}</h1>"
        if !date.empty?
          html += "<p class='article-date'>#{date}</p>"
        end
        html += "</div>"
        html += "<div class='article-content'>"
        html += Markdown.new(content).to_html
        html += "</div>"
        html += "<div class='article-footer'>"
        html += "<a href='./' class='back-link'>"
        html += "<svg class='back-link-icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M10 19l-7-7m0 0l7-7m-7 7h18'/></svg>"
        html += "Back to Home"
        html += "</a>"
        html += "</div>"
        html += "</article>"
        @content_div.innerHTML = html
        highlight_code
      else
        render_404("Error loading article", "An error occurred while loading the article (HTTP #{status}).")
      end
    end
  end

  def render_static_page(page_path)
    @window.fetch("../#{page_path}") do |response|
      status = response.status.to_i
      if status == 404
        render_404("Page not found", "The page you are looking for does not exist.")
      elsif status >= 200 && status < 300
        markdown_text = response.to_binary
        meta, content = parse_front_matter(markdown_text)
        title = meta['title'] || 'Page'

        html = "<article>"
        html += "<div class='article-header'>"
        html += "<h1 class='article-title'>#{title}</h1>"
        html += "</div>"
        html += "<div class='article-content'>"
        html += Markdown.new(content).to_html
        html += "</div>"
        html += "<div class='article-footer'>"
        html += "<a href='/' class='back-link'>"
        html += "<svg class='back-link-icon' fill='none' stroke='currentColor' viewBox='0 0 24 24'><path stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M10 19l-7-7m0 0l7-7m-7 7h18'/></svg>"
        html += "Back to Home"
        html += "</a>"
        html += "</div>"
        html += "</article>"
        @content_div.innerHTML = html
        highlight_code
      else
        render_404("Error loading page", "An error occurred while loading the page (HTTP #{status}).")
      end
    end
  end

  def highlight_code
    prism = JS.global[:Prism]
    prism.highlightAll if prism
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
