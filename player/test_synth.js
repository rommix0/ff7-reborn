// node player/test_synth.js
const assert = require("node:assert");
const { AkaoPlayer } = require("./synth.js");

const RATE = 44100;
const TIMER_HZ = 33868800 / 8 / 0x43d1;
const steady = [0x00, 0x0f, 0x0f, 0x7f, 0x06, 1, 3, 3]; // instant attack, sustain held, quick release
const tone = Int16Array.from({ length: 64 }, (_, i) => Math.round(Math.sin((2 * Math.PI * i) / 64) * 20000));
const bank = [{ slot: 0, pcm: tone, loops: true, loopStart: 0, pitches: Array(12).fill(0x1000), adsr: steady }];

function song(cells, { ticks = 192, loop = null } = {}) {
  return {
    format: "ff7song", version: 1, song: 1, channels: 1, ticks_per_quarter: 48, timer_hz: TIMER_HZ, loop,
    patterns: [{ ticks, beat: 48, beats: 4, cells }], order: [0], restart: 0,
  };
}

function player(s) {
  const p = new AkaoPlayer(RATE);
  p.setBank(bank);
  p.load(s);
  p.playing = true;
  return p;
}

function run(p, frames) {
  const l = new Float32Array(frames), r = new Float32Array(frames);
  p.render(l, r, frames);
  return { l, r, peak: Math.max(...l.map(Math.abs)) };
}

// LFO waveforms rebuilt from their shapes: the usual vibrato sine (6) has 39 steps a cycle, fading in over four.
const sine = AkaoPlayer.lfoWave(6);
assert.equal(sine.steps.length, 156);
assert.equal(sine.loop, 117);
assert.equal(Math.max(...sine.steps.slice(0, 39)), 8191);
assert.equal(AkaoPlayer.lfoWave(12), null);

// Timing: tempo 0x6000 adds 0x6000 to a 16-bit accumulator per timer interrupt; tick 96 comes 256 interrupts in.
let p = player(song([[0, 0, { tempo: 0x6000, inst: 0 }], [96, 0, { note: 60, len: 48 }]]));
run(p, Math.floor((255 * RATE) / TIMER_HZ));
assert.equal(p.tick, 95);
assert.equal(p.channels[0].voice, null);
run(p, Math.ceil((2 * RATE) / TIMER_HZ));
assert.equal(p.tick, 96);
assert.equal(p.channels[0].voice.key, 60);

// The note sounds, then releases to silence after its 48 ticks.
assert.ok(run(p, 4000).peak > 0.05);
run(p, Math.ceil((48 * 65536 / 0x6000 * RATE) / TIMER_HZ) + 2000);
assert.equal(p.channels[0].voice.phase, 4);
assert.equal(run(p, 1000).peak, 0);

// Loop: at loop.end the song jumps back to loop.start.
p = player(song([[0, 0, { tempo: 0x6000, inst: 0, note: 60, len: 48 }]], { ticks: 96, loop: { start: 48, end: 96 } }));
run(p, Math.ceil((100 * 65536 / 0x6000 * RATE) / TIMER_HZ));
assert.ok(p.tick >= 48 && p.tick < 60, p.tick); // ~100 ticks played: past 96, back to around 52

// Vibrato 7 (full-scale sine), depth 0x7F: the pitch swings +0.98 / -0.51 semitone around 0x1000 (C-6).
p = player(song([[0, 0, { tempo: 0x6000, inst: 0, vibrato: [0, 1, 7, 0x7f], note: 84, len: 192 }]]));
const seen = new Set();
for (let i = 0; i < 200; i++) {
  run(p, 128);
  const v = p.channels[0].voice;
  const off = Math.floor((v.vibrato.amount * v.vibrato.value) / 65536);
  seen.add(off > 0 ? 2 * off : off);
}
const semis = (o) => 12 * Math.log2((0x1000 + o) / 0x1000);
assert.ok(Math.abs(semis(Math.max(...seen)) - 0.98) < 0.05 && Math.abs(semis(Math.min(...seen)) + 0.51) < 0.05);

// Seeking keeps the state the sequence set before the target, and a note right on the target still plays.
p = player(song([[0, 0, { tempo: 0x6000, inst: 0, volume: 50 }], [96, 0, { note: 60, len: 48 }]]));
p.seek(96);
assert.ok(p.playing && p.tick === 95 && p.channels[0].volume === 50 && p.tempo === 0x6000);
run(p, Math.ceil((2 * 65536 / 0x6000 * RATE) / TIMER_HZ));
assert.equal(p.channels[0].voice.key, 60);

// Muted channels are silent.
p = player(song([[0, 0, { tempo: 0x6000, inst: 0, note: 60, len: 192 }]]));
p.muted = 1;
assert.equal(run(p, 8000).peak, 0);
console.log("ok");
