require "digest/sha2"
require "fileutils"
require "open3"

namespace :rbenv do
  def picoruby_version
    File.read("#{MRUBY_ROOT}/include/version.h")[/"(\d+\.\d+\.\d+)"/, 1]
  end

  desc "Create a release tarball with submodules for rbenv/ruby-build"
  task :release do
    version = picoruby_version
    tag = version
    tarball_name = "picoruby-#{version}.tar.gz"
    prefix = "picoruby-#{version}"
    tmpdir = "#{MRUBY_ROOT}/tmp/rbenv-release"

    # Verify tag exists on remote
    remote_tags = `git -C #{MRUBY_ROOT} ls-remote --tags origin #{tag}`.strip
    if remote_tags.empty?
      abort "Tag '#{tag}' not found on remote. Push it first:\n  git tag #{tag} && git push origin #{tag}"
    end

    # Submodules not needed for POSIX build (r2p2-specific)
    exclude_submodules = %w[
      mrbgems/picoruby-r2p2/lib/pico-sdk
      mrbgems/picoruby-r2p2/lib/pico-extras
      mrbgems/picoruby-r2p2/lib/rp2040js
    ]

    rm_rf tmpdir
    mkdir_p tmpdir

    puts "==> Exporting picoruby #{version}..."
    sh "git -C #{MRUBY_ROOT} archive --format=tar --prefix=#{prefix}/ HEAD | tar xf - -C #{tmpdir}"

    dest = "#{tmpdir}/#{prefix}"

    puts "==> Populating submodules..."
    # Read .gitmodules from repo
    gitmodules = File.read("#{MRUBY_ROOT}/.gitmodules")
    gitmodules.scan(/\[submodule "(.+?)"\].*?path = (.+?)$/m).each do |_name, path|
      path = path.strip
      next if exclude_submodules.include?(path)

      src = "#{MRUBY_ROOT}/#{path}"
      dst = "#{dest}/#{path}"

      unless File.exist?(src)
        puts "   SKIP #{path} (not initialized locally)"
        next
      end

      puts "   COPY #{path}"
      rm_rf dst
      cp_r src, dst

      # Handle nested submodules (e.g. mruby-compiler2/lib/prism)
      nested_gitmodules = "#{src}/.gitmodules"
      if File.exist?(nested_gitmodules)
        File.read(nested_gitmodules).scan(/path = (.+?)$/).each do |nested_path,|
          nested_path = nested_path.strip
          nested_src = "#{src}/#{nested_path}"
          nested_dst = "#{dst}/#{nested_path}"
          if File.exist?(nested_src)
            puts "   COPY #{path}/#{nested_path}"
            rm_rf nested_dst
            cp_r nested_src, nested_dst
          end
        end
      end
    end

    # Remove all .git directories
    Dir.glob("#{dest}/**/.git", File::FNM_DOTMATCH).each do |git_dir|
      rm_rf git_dir
    end

    tarball_path = "#{tmpdir}/#{tarball_name}"
    puts "==> Creating #{tarball_name}..."
    sh "tar czf #{tarball_path} -C #{tmpdir} #{prefix}"

    sha256 = Digest::SHA256.file(tarball_path).hexdigest
    puts
    puts "Tarball: #{tarball_path}"
    puts "SHA256:  #{sha256}"
    puts

    # Upload to GitHub Release (release must already exist)
    puts "==> Uploading to GitHub Release #{tag}..."
    sh "gh release upload #{tag} #{tarball_path} --clobber", chdir: MRUBY_ROOT

    # Get the download URL
    asset_url = "https://github.com/picoruby/picoruby/releases/download/#{tag}/#{tarball_name}"
    definition = "install_package \"picoruby-#{version}\" \"#{asset_url}##{sha256}\" picoruby_r2p2"

    puts
    puts "==> ruby-build definition:"
    puts definition
    puts
    puts "Write this to: share/ruby-build/picoruby-#{version}"

    rm_rf tmpdir
  end
end
