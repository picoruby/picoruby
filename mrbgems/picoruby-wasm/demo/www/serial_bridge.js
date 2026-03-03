(() => {
  class SerialBridge {
    constructor(win) {
      this.win = win;
      this.state = {
        port: null,
        autoReconnect: false,
      };
    }

    isPortConnected() {
      return this.state.port !== null;
    }
  }

  window.serialBridge = new SerialBridge(window);

})();
