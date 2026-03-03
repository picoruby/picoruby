(() => {
  class SerialBridge {
    constructor(win) {
      this.win = win;
      this.state = {
        port: null,
        reader: null,
        autoReconnect: false,
      };
      this.terminal = null;
    }

    setTerminal(terminal) {
      this.terminal = terminal;
    }

    isPortConnected() {
      return this.state.port !== null;
    }

    async readFromPort() {
      const port = this.state.port;
      if (!port) return;
      const decoder = new TextDecoder("utf-8");
      const activeReader = port.readable.getReader();
      this.state.reader = activeReader;
      try {
        while (true) {
          const { value, done } = await activeReader.read();
          if (done) break;
          if (value) {
            const chars = decoder.decode(value, { stream: true });
            const capture = globalThis.picorubySerialCapture;
            if (capture && port) capture.append(port, chars);
            if (this.terminal) this.terminal.write(chars);
          }
        }
      } catch (err) {
        console.error("Serial read error:", err);
      } finally {
        activeReader.releaseLock();
        if (this.state.reader === activeReader) this.state.reader = null;
        this.win.dispatchEvent(new CustomEvent("serial-reader-closed"));
      }
    }
  }

  window.serialBridge = new SerialBridge(window);

})();
