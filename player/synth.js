// AKAO driver emulation for .ff7song files (docs/ff7song_format.md). The whole class is turned into the
// AudioWorklet's source with toString(), so it must not reference anything outside itself.
class AkaoPlayer {
  constructor(sampleRate) {
    this.sampleRate = sampleRate;
    this.bank = new Map();
    this.song = null;
    this.muted = 0;
  }

  // Waveform n of the driver's LFO table, rebuilt from its shape (knowledge_base.md, "LFOs"): {steps, loop}.
  static lfoWave(n) {
    n = { 13: 9, 14: 8, 15: 9 }[n] ?? n;
    if (n > 11) return null; // 12 is a noise table we don't carry
    const range = (k) => [...Array(k).keys()];
    const cycle = {
      0: [1, -1], 2: [1, 0], 4: [-1, 0],
      6: range(40).filter((i) => i !== 3).map((i) => Math.sin((Math.PI * i) / 20)), // the game skips step 4
      8: range(16).map((i) => (i < 8 ? (4 - Math.abs(i - 4)) / 4 : (Math.abs(i - 12) - 4) / 4)),
      10: range(20).map((i) => Math.sin((Math.PI * i) / 10)),
    }[n & ~1];
    const scales = n % 2 === 0 ? [8191, 16383, 24575, 32767] : [32767]; // even ones fade in over four cycles
    const steps = scales.flatMap((s) => cycle.map((v) => Math.round(s * v)));
    return { steps, loop: steps.length - cycle.length };
  }

  // instruments: [{slot, pcm: Int16Array, loops, loopStart, pitches[12], adsr[8]}]
  setBank(instruments) {
    this.bank = new Map(instruments.map((i) => [i.slot, { ...i, pcm: Float32Array.from(i.pcm, (s) => s / 32768) }]));
  }

  load(song) {
    this.song = song;
    this.barStarts = [];
    let t = 0;
    for (const p of song.order) {
      this.barStarts.push(t);
      t += song.patterns[p].ticks;
    }
    this.end = t;
    this.events = []; // [tick, channel, fields], in time order
    song.order.forEach((p, i) => {
      for (const [tick, ch, fields] of song.patterns[p].cells) this.events.push([this.barStarts[i] + tick, ch, fields]);
    });
    this.events.sort((a, b) => a[0] - b[0]);
    this.reset();
  }

  reset() {
    this.tick = -1;
    this.next = 0; // index of the next event
    this.tempo = 0;
    this.accumulator = 0;
    this.untilInterrupt = 0;
    this.envClock = 0;
    this.playing = false;
    this.ended = false;
    this.channels = Array.from({ length: this.song.channels }, () => ({
      inst: 5, volume: 100, expression: 120, pan: 64, detune: 0, vibrato: null, tremolo: null, voice: null,
    }));
  }

  // Jump to `target` by running the sequence silently up to the tick before it, so volume, instrument and tempo
  // are what they would be and notes on `target` itself still play.
  seek(target) {
    const playing = this.playing;
    this.reset();
    while (this.tick < target - 1 && !this.ended) this.stepTick();
    for (const ch of this.channels) ch.voice = null;
    this.playing = playing;
  }

  pitchOf(ch, key) {
    const inst = this.bank.get(ch.inst);
    if (!inst) return 0;
    const pitch = inst.pitches[key % 12] * 2 ** (Math.floor(key / 12) - 1 - 6);
    return pitch * (1 + ch.detune / (ch.detune > 0 ? 128 : 256));
  }

  // The driver restarts both LFOs with every note: `delay` ticks, then a waveform step every `rate` interrupts.
  startLfo(settings) {
    if (!settings || !settings[3]) return null;
    const [delay, rate, waveform, depth] = settings;
    const wave = AkaoPlayer.lfoWave(waveform);
    return wave && { delay, rate, depth, wave, countdown: 1, index: 0, value: 0 };
  }

  stepLfo(lfo) {
    if (!lfo || lfo.delay > 0 || --lfo.countdown > 0) return;
    lfo.countdown = lfo.rate;
    const { steps, loop } = lfo.wave;
    lfo.value = steps[lfo.index];
    lfo.index = lfo.index + 1 < steps.length ? lfo.index + 1 : loop;
  }

  keyOn(ch, key, length) {
    const inst = this.bank.get(ch.inst);
    if (!inst) {
      ch.voice = null;
      return;
    }
    const pitch = this.pitchOf(ch, key);
    const vibrato = this.startLfo(ch.vibrato);
    if (vibrato) {
      // Depth as a share of the note's pitch: (d & 0x7F)/128 of it, or 15/256 of that without d's top bit.
      const d = vibrato.depth;
      vibrato.amount = ((d & 0x7f) * (d & 0x80 ? pitch : (pitch * 15) / 256)) / 128;
    }
    ch.voice = {
      inst, key, pitch, left: length, pos: 0, bend: null, vibrato, tremolo: this.startLfo(ch.tremolo),
      phase: 0, level: 0, envCount: 0, // envelope phases: 0 attack, 1 decay, 2 sustain, 3 release, 4 off
    };
  }

  stepTick() {
    this.tick++;
    if (this.song.loop && this.tick >= this.song.loop.end) {
      this.tick = this.song.loop.start;
      this.next = this.events.findIndex((e) => e[0] >= this.tick);
    } else if (this.tick >= this.end) {
      this.ended = true;
      return;
    }
    for (const ch of this.channels) {
      const v = ch.voice;
      if (!v) continue;
      if (v.left > 0 && --v.left === 0 && v.phase < 3) v.phase = 3;
      for (const lfo of [v.vibrato, v.tremolo]) if (lfo && lfo.delay > 0) lfo.delay--;
      if (v.bend && v.bend.done < v.bend.ticks) {
        v.bend.done++;
        v.pitch = v.bend.from + ((v.bend.to - v.bend.from) * v.bend.done) / v.bend.ticks; // linear in frequency
      }
    }
    while (this.next >= 0 && this.next < this.events.length && this.events[this.next][0] === this.tick) {
      const [, c, f] = this.events[this.next++];
      const ch = this.channels[c];
      for (const k of ["inst", "volume", "expression", "pan", "detune", "vibrato", "tremolo"]) if (k in f) ch[k] = f[k];
      if ("tempo" in f) this.tempo = f.tempo;
      if (f.bend && ch.voice) {
        const [ticks, semitones] = f.bend;
        ch.voice.bend = { from: ch.voice.pitch, to: this.pitchOf(ch, ch.voice.key + semitones), ticks, done: 0 };
        ch.voice.key += semitones;
      }
      if ("note" in f) this.keyOn(ch, f.note, f.len);
    }
  }

  interrupt() {
    for (const ch of this.channels) if (ch.voice) for (const lfo of [ch.voice.vibrato, ch.voice.tremolo]) this.stepLfo(lfo);
    if (this.tick < 0) return this.stepTick(); // the song's first tick sets its tempo
    this.accumulator += this.tempo;
    if (this.accumulator >= 0x10000) {
      this.accumulator -= 0x10000;
      this.stepTick();
    }
  }

  // One step of the SPU envelope at 44.1 kHz (psx-spx ADSR rules); `adsr` is AR DR SL SR RR and the three modes.
  stepEnvelope(v) {
    const [ar, dr, sl, sr, rr, aMode, sMode, rMode] = v.inst.adsr;
    let shift, step, exponential, rising;
    switch (v.phase) {
      case 0: [shift, step, exponential, rising] = [ar >> 2, 7 - (ar & 3), aMode === 5, true]; break;
      case 1: [shift, step, exponential, rising] = [dr, -8, true, false]; break;
      case 2:
        rising = sMode === 1 || sMode === 5;
        [shift, step, exponential] = [sr >> 2, rising ? 7 - (sr & 3) : -8 + (sr & 3), sMode === 5 || sMode === 7];
        break;
      case 3: [shift, step, exponential, rising] = [rr, -8, rMode === 7, false]; break;
      default: return;
    }
    let cycles = 1 << Math.max(0, shift - 11);
    if (exponential && rising && v.level > 0x6000) cycles *= 4;
    if (++v.envCount < cycles) return;
    v.envCount = 0;
    let delta = step << Math.max(0, 11 - shift);
    if (exponential && !rising) delta = (delta * v.level) >> 15;
    v.level = Math.max(0, Math.min(0x7fff, v.level + delta));
    if (v.phase === 0 && v.level >= 0x7fff) v.phase = 1;
    else if (v.phase === 1 && v.level <= Math.min(0x7fff, (sl + 1) * 0x800)) v.phase = 2;
    else if (v.phase === 3 && v.level === 0) v.phase = 4;
  }

  render(left, right, frames) {
    left.fill(0);
    right.fill(0);
    if (!this.song || !this.playing) return;
    const perInterrupt = this.sampleRate / this.song.timer_hz;
    for (let i = 0; i < frames; i++) {
      if (--this.untilInterrupt <= 0) {
        this.untilInterrupt += perInterrupt;
        this.interrupt();
      }
      this.envClock += 44100 / this.sampleRate;
      const envSteps = Math.floor(this.envClock);
      this.envClock -= envSteps;
      let l = 0, r = 0;
      this.channels.forEach((ch, c) => {
        const v = ch.voice;
        if (!v || v.phase === 4) return;
        for (let s = 0; s < envSteps; s++) this.stepEnvelope(v);
        const { pcm, loops, loopStart } = v.inst;
        const at = Math.floor(v.pos);
        if (at >= pcm.length) {
          v.phase = 4;
          return;
        }
        const after = at + 1 < pcm.length ? at + 1 : loops ? loopStart : at;
        const sample = pcm[at] + (pcm[after] - pcm[at]) * (v.pos - at);
        let pitch = v.pitch;
        if (v.vibrato && v.vibrato.amount) {
          const offset = Math.floor((v.vibrato.amount * v.vibrato.value) / 65536);
          pitch += offset > 0 ? 2 * offset : offset; // the driver doubles the upward half
        }
        v.pos += (Math.min(0x3fff, Math.max(0, pitch)) / 4096) * (44100 / this.sampleRate);
        if (v.pos >= pcm.length && loops) v.pos -= pcm.length - loopStart;
        if ((this.muted >> c) & 1) return;
        let gain = (v.level / 0x7fff) * ((ch.volume * ch.expression) / (127 * 127)) * 0.5;
        if (v.tremolo) gain *= Math.max(0, 1 + ((v.tremolo.depth / 128) * v.tremolo.value) / 32768);
        const angle = (ch.pan / 127) * (Math.PI / 2);
        l += sample * gain * Math.cos(angle);
        r += sample * gain * Math.sin(angle);
      });
      left[i] = Math.max(-1, Math.min(1, l));
      right[i] = Math.max(-1, Math.min(1, r));
      if (this.ended && this.channels.every((ch) => !ch.voice || ch.voice.phase === 4)) this.playing = false;
    }
  }
}

if (typeof module !== "undefined") module.exports = { AkaoPlayer };
