# PicoRuby Shuttle Blog Example

This is an example blog site built with PicoRuby Shuttle, a static site generator powered by PicoRuby WASM.

## Features

- Static site generation with PicoRuby WASM
- Query string-based routing (SEO-friendly)
- GitHub Pages compatible
- Markdown article support with front matter
- Automatic article indexing

## Project Structure

```
.
├── .github/
│   └── workflows/
│       └── deploy.yml       # GitHub Actions workflow for deployment
├── articles/                # Your markdown articles
│   ├── my_first_post.md
│   └── about_picoruby.md
├── dist/                    # Generated site (output directory)
│   ├── index.html
│   ├── 404.html
│   └── articles/
├── Rakefile                 # Build tasks
└── README.md
```

## Getting Started

### Local Development

1. Clone this repository
2. Add your articles to the `articles/` directory
3. Generate the article index:
   ```bash
   rake shuttle:generate_index
   ```
4. Start the local server:
   ```bash
   rake shuttle:server
   ```
5. Open http://localhost:8000 in your browser

### Writing Articles

Create markdown files in the `articles/` directory with YAML front matter:

```markdown
---
title: My First Post
date: 2025-12-24
---

This is the content of my first post.
```

### Deployment to GitHub Pages

This project is configured to automatically deploy to GitHub Pages using GitHub Actions.

1. Push your repository to GitHub
2. Enable GitHub Pages in your repository settings:
   - Go to Settings > Pages
   - Source: GitHub Actions
3. Push to the main/master branch to trigger deployment

The workflow will:
- Generate article index
- Download PicoRuby WASM from npm
- Build and deploy to GitHub Pages

## URL Structure

- `/` - Home page (article list, page 1)
- `/?page=2` - Article list, page 2
- `/?article=my_first_post` - Individual article

## Build Tasks

```bash
# Generate article index and copy markdown files
rake shuttle:generate_index

# Build site for deployment
rake shuttle:build

# Start local development server
rake shuttle:server
```

## Requirements

- Ruby 3.x
- Rake
- JSON gem

For GitHub Actions deployment, no additional setup is required.

## License

MIT
