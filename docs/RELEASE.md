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
