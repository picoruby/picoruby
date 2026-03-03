(() => {
  class SerialBridge {
    constructor(win) {
      this.win = win;
      this.state = {
        port: null,
        reader: null,
        autoReconnect: false,
        captureMode: false,
        captureBuffer: "",
      };
      this.terminal = null;
    }

    setTerminal(terminal) {
      this.terminal = terminal;
    }

    isPortConnected() {
      return this.state.port !== null;
    }

    startCapture() {
      this.state.captureBuffer = "";
      this.state.captureMode = true;
    }

    peekCapture() {
      return this.state.captureBuffer;
    }

    stopAndGetCapture() {
      this.state.captureMode = false;
      return this.state.captureBuffer;
    }

    async writeBytes(bytes) {
      const port = this.state.port;
      if (!port || !port.writable) return;
      const writer = port.writable.getWriter();
      try {
        await writer.write(bytes);
      } catch (err) {
        console.error("Serial write error:", err);
      } finally {
        writer.releaseLock();
      }
    }

    sendTextToPort(str) {
      const encoder = new TextEncoder();
      return this.writeBytes(encoder.encode(String(str)));
    }

    async sendHexToPort(hex) {
      const len = hex.length / 2;
      const bytes = new Uint8Array(len);
      for (let i = 0; i < len; i++) {
        bytes[i] = parseInt(hex.slice(i * 2, i * 2 + 2), 16);
      }
      const chunk = 64;
      for (let off = 0; off < bytes.length; off += chunk) {
        const end = Math.min(off + chunk, bytes.length);
        await this.writeBytes(bytes.subarray(off, end));
        if (end < bytes.length) {
          await new Promise((r) => setTimeout(r, 10));
        }
      }
    }

    async requestConnect() {
      if (this.state.port) {
        this.win.dispatchEvent(new CustomEvent("serial-disconnect-request"));
        return;
      }
      try {
        this.win.pendingSerialPort = await navigator.serial.requestPort();
        this.win.dispatchEvent(new CustomEvent("serial-connect-request"));
      } catch (err) {
        console.error("Connect error:", err);
        const message = err && err.message ? err.message : String(err);
        this.win.lastSerialConnectError = message;
        this.win.dispatchEvent(new CustomEvent("serial-connect-error"));
      }
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
            if (this.state.captureMode) this.state.captureBuffer += chars;
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

  Object.defineProperty(window, "port", {
    get() { return window.serialBridge.state.port; },
    set(v) { window.serialBridge.state.port = v; },
    configurable: true,
  });
  Object.defineProperty(window, "reader", {
    get() { return window.serialBridge.state.reader; },
    set(v) { window.serialBridge.state.reader = v; },
    configurable: true,
  });
  Object.defineProperty(window, "autoReconnect", {
    get() { return window.serialBridge.state.autoReconnect; },
    set(v) { window.serialBridge.state.autoReconnect = !!v; },
    configurable: true,
  });

  window.isPortConnected = () => window.serialBridge.isPortConnected();
  window.startCapture = () => window.serialBridge.startCapture();
  window.peekCapture = () => window.serialBridge.peekCapture();
  window.stopAndGetCapture = () => window.serialBridge.stopAndGetCapture();
  window.sendTextToPort = (str) => window.serialBridge.sendTextToPort(str);
  window.sendHexToPort = (hex) => window.serialBridge.sendHexToPort(hex);
  window.readFromPort = () => window.serialBridge.readFromPort();
})();
