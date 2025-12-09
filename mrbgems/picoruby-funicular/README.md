# Funicular

> 🎵Funicu-lì, Funicu-là!🚊🚊🚊
>
> 🎵Funicu-lì, Funicu-là!🚞🚞🚞


**Funicular** is an SPA framework powered by PicoRuby.wasm, featuring:
- Pure Ruby with no JavaScript or HTML required[^1]
- Browser-native *Object-REST Mapper* (O**R**M)
- ActionCable-compatible WebSocket support
- Seamless Rails integration
- Flexible JavaScript Integration via Delegation Model

[^1]: 子曰*知止而后有定* / Confucius, *When one knows where to stop, one can be steady.*

## Features

### Pure Ruby browser app

### Object-REST Mapper (O**R**M)

### ActionCable-compatible WebSocket

### Rails integration

### JS Integration via Delegation Model

For complex UI interactions or data visualizations requiring existing JavaScript libraries (e.g., Chart.js, D3.js), Funicular adopts a "delegation model."
This strategy allows you to define container DOM elements within your Ruby components using `ref` attributes.
During lifecycle hooks (like `component_mounted`), control of these specific DOM elements is "delegated" to external JavaScript code.
This ensures you can leverage the full power and rich ecosystems of existing JS libraries, maintain a clear separation of concerns between Ruby (component structure, data flow) and JavaScript (DOM manipulation, library-specific logic), and keep the Funicular core lean and focused on Ruby-first development.

### License

MIT
