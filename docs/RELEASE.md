# Release Guide

This document is intended for maintainers of the PicoRuby project.

## WASM packages (npm)

Both `@picoruby/wasm` and `@picoruby/picorbc` are published from
`mrbgems/picoruby-wasm/npm/picoruby/` and `mrbgems/picoruby-wasm/npm/picorbc/`
respectively.

### Prerequisites

- Emscripten SDK (emcc) installed and available in PATH
- npm account with publish access to `@picoruby` scope
- Logged in to npm (`npm whoami` to verify)

### Build and publish

```bash
rake wasm:release
```

This runs the following steps automatically:

1. `CONFIG=picoruby-wasm rake clean`
2. `CONFIG=picorbc-wasm rake clean`
3. `CONFIG=picoruby-wasm rake` (build picoruby.wasm)
4. `CONFIG=picorbc-wasm rake` (build picorbc.wasm)
5. Copy picorbc artifacts to `mrbgems/picoruby-wasm/npm/picorbc/dist/`
6. `npm publish --access public` for both packages

### Verify

```bash
rake wasm:versions
```

## rbenv / ruby-build

PicoRuby can be installed via `rbenv install picoruby-X.Y.Z`.

GitHub's default source archive (`/archive/<tag>.tar.gz`) does not include
git submodules. Since PicoRuby depends on several submodules
(mruby-compiler2, prism, mrubyc, etc.), `rake rbenv:release` creates a
self-contained tarball with all required submodules bundled, uploads it to
the GitHub Release, and prints the ruby-build definition with a SHA256
checksum.

### Prerequisites

- `gh` CLI authenticated with push access to picoruby/picoruby
- All submodules initialized locally (`git submodule update --init --recursive`)
- Version tag pushed to remote

### Steps

1. Update version in `include/version.h`
2. Commit and push to master
3. Create and push a tag:
   ```bash
   git tag 3.4.2
   git push origin 3.4.2
   ```
4. Create a GitHub Release (can be draft initially):
   ```bash
   gh release create 3.4.2 --title 'PicoRuby 3.4.2' --notes 'PicoRuby 3.4.2' --draft
   ```
5. Build and upload the tarball:
   ```bash
   rake rbenv:release
   ```
6. Publish the release:
   ```bash
   gh release edit 3.4.2 --draft=false
   ```
7. Copy the printed ruby-build definition to the ruby-build plugin:
   ```
   share/ruby-build/picoruby-3.4.2
   ```

### What `rake rbenv:release` does

1. Verifies the version tag exists on the remote (aborts if not)
2. Exports the source tree via `git archive`
3. Copies submodules into the export (excluding r2p2-specific ones:
   pico-sdk, pico-extras, rp2040js)
4. Strips all `.git` directories
5. Creates a `.tar.gz` and computes SHA256
6. Uploads to the GitHub Release via `gh release upload --clobber`
7. Prints the `install_package` line for ruby-build

### Verify

```bash
rbenv install picoruby-3.4.2
rbenv shell picoruby-3.4.2
ruby --version
```
