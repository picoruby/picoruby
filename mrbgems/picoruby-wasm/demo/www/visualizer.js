(function (global) {
  const COLORS = [
    '#5eead4', '#60a5fa', '#c084fc', '#fb7185',
    '#facc15', '#4ade80', '#38bdf8', '#f472b6',
    '#a3e635', '#f97316', '#818cf8', '#2dd4bf',
    '#e879f9', '#fda4af', '#bef264', '#67e8f9'
  ];

  class PicoRubyWebAudioVisualizer {
    constructor(canvas, mixAnalyser, voiceAnalysers) {
      this.canvas = canvas;
      this.context = canvas.getContext('2d');
      this.mix = mixAnalyser;
      this.voices = Array.from(voiceAnalysers || []);
      this.running = false;
      this.lastFrame = 0;
      this.frame = this.frame.bind(this);
      this.buffers = new Map();
    }

    start() {
      if (this.running) return;
      this.running = true;
      requestAnimationFrame(this.frame);
    }

    stop() {
      this.running = false;
    }

    resize() {
      const ratio = global.devicePixelRatio || 1;
      const width = Math.max(1, this.canvas.clientWidth);
      const height = Math.max(1, this.canvas.clientHeight);
      const pixelWidth = Math.floor(width * ratio);
      const pixelHeight = Math.floor(height * ratio);
      if (this.canvas.width !== pixelWidth || this.canvas.height !== pixelHeight) {
        this.canvas.width = pixelWidth;
        this.canvas.height = pixelHeight;
      }
      this.context.setTransform(ratio, 0, 0, ratio, 0, 0);
      return [width, height];
    }

    samples(analyser) {
      let data = this.buffers.get(analyser);
      const length = analyser.frequencyBinCount;
      if (!data || data.length !== length) {
        data = new Uint8Array(length);
        this.buffers.set(analyser, data);
      }
      analyser.getByteTimeDomainData(data);
      return data;
    }

    drawWave(analyser, color, width, height, lineWidth, skipFlat) {
      const data = this.samples(analyser);
      let energy = 0;
      for (let i = 0; i < data.length; i++) energy += Math.abs(data[i] - 128);
      if (skipFlat && energy < data.length * 0.35) return;

      const ctx = this.context;
      ctx.beginPath();
      ctx.strokeStyle = color;
      ctx.lineWidth = lineWidth;
      ctx.globalAlpha = skipFlat ? 0.8 : 0.95;
      for (let i = 0; i < data.length; i++) {
        const x = i * width / Math.max(1, data.length - 1);
        const y = data[i] * height / 256;
        if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }

    frame(time) {
      if (!this.running) return;
      requestAnimationFrame(this.frame);
      if (time - this.lastFrame < 33) return;
      this.lastFrame = time;
      const [width, height] = this.resize();
      const ctx = this.context;
      ctx.globalAlpha = 1;
      ctx.fillStyle = '#07111f';
      ctx.fillRect(0, 0, width, height);
      ctx.strokeStyle = '#19324a';
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(0, height / 2);
      ctx.lineTo(width, height / 2);
      ctx.stroke();

      for (let i = 0; i < this.voices.length; i++) {
        this.drawWave(this.voices[i], COLORS[i % COLORS.length], width, height, 1, true);
      }
      this.drawWave(this.mix, '#f8fafc', width, height, 2.2, false);
      ctx.globalAlpha = 1;
    }
  }

  global.PicoRubyWebAudioVisualizer = PicoRubyWebAudioVisualizer;
})(globalThis);
