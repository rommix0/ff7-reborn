#ifndef SOUND_H
#define SOUND_H

/* SeqChannel.flags */
#define CHANNEL_VIBRATO 0x1
#define CHANNEL_TREMOLO 0x2
#define CHANNEL_PAN_LFO 0x4
#define CHANNEL_DRUMS   0x8
#define CHANNEL_PITCH_FOLLOW  0x10 /* D4: pitch register = the previous channel's */
#define CHANNEL_VOLUME_FOLLOW 0x20 /* D6: volume follows the previous channel's pitch */
#define CHANNEL_LAYER         0x100 /* F4: a second voice under this one */
#define CHANNEL_OVERLAP       0x200 /* F8: notes take turns between two voices */

/* SeqSong.output_mode: how a channel's volume splits between the sides */
#define OUTPUT_STEREO 1 /* by its pan */
#define OUTPUT_MONO   2 /* both sides at the centre's share (any value but the other two) */
#define OUTPUT_INVERT 4 /* by its pan, one side inverted: the right on odd voices, the left on even ones */

/* snd_flags */
#define SND_MUSIC_PAUSED   0x1   /* 9B */
#define SND_EFFECTS_PAUSED 0x2   /* 9D: only a kind 2 effect keeps stepping */
#define SND_DOUBLE_SPEED   0x4   /* never set by the driver: two sequencer steps per timer tick, on every tick */
#define SND_REVERB_SCALED  0x10  /* E4 with a non-zero value */
#define SND_EFFECTS_ASIDE  0x100 /* F4 set the effects aside in channels 24-31 */

/* SeqSong.flags */
#define SONG_UPDATE_NOISE  0x10 /* the noise voices or clock changed */
#define SONG_UPDATE_REVERB 0x80 /* the reverb depth changed */

/* VoiceSettings.update: which SPU voice settings snd_update_voice writes next (libspu's voice attribute mask) */
#define UPDATE_VOLUME        0x3 /* both sides */
#define UPDATE_PITCH         0x10
#define UPDATE_SAMPLE        0x80 /* the sample's start address */
#define UPDATE_ATTACK_MODE   0x100
#define UPDATE_SUSTAIN_MODE  0x200
#define UPDATE_RELEASE_MODE  0x400
#define UPDATE_ATTACK_RATE   0x800
#define UPDATE_DECAY_RATE    0x1000
#define UPDATE_SUSTAIN_RATE  0x2000
#define UPDATE_RELEASE_RATE  0x4000
#define UPDATE_SUSTAIN_LEVEL 0x8000
#define UPDATE_ENVELOPE      0xFF00 /* all of the above from the attack mode on */
#define UPDATE_LOOP          0x10000 /* the sample's loop address */
#define UPDATE_ALL           (UPDATE_VOLUME | UPDATE_PITCH | UPDATE_SAMPLE | UPDATE_ENVELOPE | UPDATE_LOOP)

/* A voice's settings as snd_update_voice hands them to libspu: a channel keeps them at +0xDC. */
typedef struct VoiceSettings {
    unsigned int voice;                 /* 0x00 the SPU voice */
    unsigned int update;                /* 0x04 UPDATE_*: which settings to write next */
    unsigned int sample;                /* 0x08 the instrument's SPU sample address */
    unsigned int loop;                  /* 0x0C and loop address */
    unsigned int attack_mode;           /* 0x10 B7 */
    unsigned int sustain_mode;          /* 0x14 BB */
    unsigned int release_mode;          /* 0x18 BF */
    unsigned short pitch;               /* 0x1C the last pitch register written */
    unsigned short attack_rate;         /* 0x1E AD */
    unsigned short decay_rate;          /* 0x20 AE */
    unsigned short sustain_level;       /* 0x22 AF */
    unsigned short sustain_rate;        /* 0x24 B1 */
    unsigned short release_rate;        /* 0x26 B2 */
    short volume[2];                    /* 0x28 left, right */
} VoiceSettings;

/* One sequence channel of the AKAO driver, 0x108 bytes (docs/knowledge_base.md §4). Fields get names as the
 * functions that use them are recovered; the pads are still unread. */
typedef struct SeqChannel {
    unsigned char *pos;                 /* 0x00 read pointer into the sequence */
    unsigned char *loop_start[4];       /* 0x04 C8, one per loop level */
    unsigned char *drum_map;            /* 0x14 EC: 5 bytes per key */
    short *vibrato_wave_pos;            /* 0x18 the LFO's place in its waveform (lfo_waveforms) */
    short *tremolo_wave_pos;            /* 0x1C */
    short *pan_lfo_wave_pos;            /* 0x20 */
    unsigned int layer_voice;           /* 0x24 F4: the SPU voice of the second voice */
    unsigned int overlap_voice;         /* 0x28 F8: the second voice notes take turns on; an effect's own voice */
    unsigned int volume_scale;          /* 0x2C A3 */
    int note_pitch;                     /* 0x30 the sounding note's pitch register (vibrato scales with it) */
    int bend_offset;                    /* 0x34 the A4 bend so far, << 16: its top half adds to the pitch register */
    unsigned int flags;                 /* 0x38 CHANNEL_* */
    int pitch_shift;                    /* 0x3C set by the game on a playing effect: a signed byte << 8 */
    int pitch_shift_step;               /* 0x40 */
    int volume;                         /* 0x44 A8 value << 23 */
    int volume_slide_step;              /* 0x48 */
    int bend_step;                      /* 0x4C bend_offset's step per tick */
    int effect_ticks;                   /* 0x50 an effect's steps since it started (from -1); the driver never reads it */
    unsigned short is_effect;           /* 0x54 a sound effect's channel */
    unsigned short gate;                /* 0x56 key-off countdown << 8 | ticks to the next event */
    unsigned short instrument;          /* 0x58 */
    unsigned short pitch_shift_slide_ticks; /* 0x5A */
    unsigned short volume_slide_ticks;  /* 0x5C */
    unsigned short balance_slide_ticks; /* 0x5E */
    unsigned short pan;                 /* 0x60 AA value << 8 */
    unsigned short pan_slide_ticks;     /* 0x62 */
    unsigned short bend_count;          /* 0x64 ticks of the bend still to go */
    unsigned short octave;              /* 0x66 */
    unsigned short bend_ticks;          /* 0x68 A4 */
    unsigned short last_key;            /* 0x6A the last note's key, 0 = none */
    unsigned short glide_ticks;         /* 0x6C DA */
    unsigned short note_mode;           /* 0x6E bit 0 slur (CC), bit 1 this note slurs from the last, bit 2 full length (D0) */
    unsigned char pad_70[0x02];
    unsigned short vibrato_delay;       /* 0x72 B4: ticks after a note before it starts */
    unsigned short vibrato_delay_count; /* 0x74 */
    unsigned short vibrato_rate;        /* 0x76 ticks per waveform step */
    unsigned short vibrato_step_count;  /* 0x78 */
    unsigned short vibrato_wave;        /* 0x7A waveform number */
    short vibrato_amount;               /* 0x7C the depth applied to the note's pitch */
    unsigned short vibrato_depth;       /* 0x7E B5 value << 8: bits 8-14 depth, bit 15 full scale */
    unsigned short vibrato_slide_ticks; /* 0x80 DD */
    short vibrato_slide_step;           /* 0x82 */
    unsigned char pad_84[0x02];
    unsigned short tremolo_delay;       /* 0x86 B8 */
    unsigned short tremolo_delay_count; /* 0x88 */
    unsigned short tremolo_rate;        /* 0x8A */
    unsigned short tremolo_step_count;  /* 0x8C */
    unsigned short tremolo_wave;        /* 0x8E */
    unsigned short tremolo_depth;       /* 0x90 B9 value << 8 */
    unsigned short tremolo_slide_ticks; /* 0x92 DE */
    short tremolo_slide_step;           /* 0x94 */
    unsigned char pad_96[0x02];
    unsigned short pan_lfo_rate;        /* 0x98 BC (no delay) */
    unsigned short pan_lfo_step_count;  /* 0x9A */
    unsigned short pan_lfo_wave;        /* 0x9C */
    unsigned short pan_lfo_depth;       /* 0x9E BD value << 7 */
    unsigned short pan_lfo_slide_ticks; /* 0xA0 DF */
    short pan_lfo_slide_step;           /* 0xA2 */
    unsigned short noise_ticks;         /* 0xA4 CE/CF: ticks until noise flips, 0 = off */
    unsigned short fm_ticks;            /* 0xA6 D2/D3: ticks until FM flips, 0 = off */
    unsigned char pad_A8[0x10];
    unsigned short loop_level;          /* 0xB8 0-3 */
    unsigned short loop_count[4];       /* 0xBA passes made at each level */
    unsigned short last_length;         /* 0xC2 the length DC counts from (A2 sets it) */
    short fixed_length;                 /* 0xC4 DC, 0 = off */
    short balance;                      /* 0xC6 F6 value << 8: the layer voice's share */
    short balance_slide_step;           /* 0xC8 */
    short pan_slide_step;               /* 0xCA */
    short transpose;                    /* 0xCC */
    short detune;                       /* 0xCE */
    unsigned short key;                 /* 0xD0 the note's key (octave * 12 + note), moved by A4 bends */
    short bend_semitones;               /* 0xD2 A4, spent by the next note or tie */
    short last_transpose;               /* 0xD4 the transpose when the last note played (a glide starts from it) */
    short vibrato_offset;               /* 0xD6 */
    short tremolo_offset;               /* 0xD8 */
    short pan_lfo_offset;               /* 0xDA */
    VoiceSettings voice;                /* 0xDC */
} SeqChannel;

/* The song or effect that owns the channels, handed to every command handler. */
typedef struct SeqSong {
    unsigned int output_mode;           /* 0x00 OUTPUT_*: sound commands 80/81/82 */
    unsigned int active_voices;         /* 0x04 the song's own channels */
    unsigned int key_on_voices;         /* 0x08 voices to key on at the next output pass */
    unsigned int update_voices;         /* 0x0C voices the driver keeps updating (key-off takes a voice out) */
    unsigned int key_off_voices;        /* 0x10 voices to key off next (snd_key_off_voices) */
    unsigned int paused_voices;         /* 0x14 the active voices while 9B has the song paused */
    unsigned int tempo;                 /* 0x18 E8 value << 16 */
    int tempo_slide_step;               /* 0x1C E9 */
    unsigned int tempo_phase;           /* 0x20 the tempo accumulator; a new song starts it at 1 */
    unsigned int layer_voices;          /* 0x24 voices held by F4 layers */
    unsigned int overlap_voices;        /* 0x28 voices held by F8 overlaps */
    unsigned int noise_voices;          /* 0x2C C4/C5 */
    unsigned int reverb_voices;         /* 0x30 C2/C3 */
    unsigned int fm_voices;             /* 0x34 C6/C7 */
    unsigned int flags;                 /* 0x38 SONG_* */
    unsigned int reverb_mode;           /* 0x3C snd_set_reverb_mode */
    int reverb_depth;                   /* 0x40 EA value << 16 */
    int reverb_slide_step;              /* 0x44 EB */
    unsigned short tempo_slide_ticks;   /* 0x48 */
    unsigned short song_id;             /* 0x4A the playing song (sound command 10); A0 clears it when the last channel ends */
    unsigned short branch_taken;        /* 0x4C EF: the n of the last branch taken */
    unsigned short branch_level;        /* 0x4E EF branches when this is non-zero and at least n (set by the game) */
    unsigned short reverb_slide_ticks;  /* 0x50 */
    unsigned short noise_clock;         /* 0x52 AC */
    unsigned short pause_pending;       /* 0x54 F3: seq_tick pauses the music (9B) at its end */
    unsigned short beats_per_bar;       /* 0x56 FD */
    unsigned short bar_beat;            /* 0x58 the bar's current beat */
    unsigned short beat_ticks;          /* 0x5A FD ticks per beat */
    unsigned short beat_tick;           /* 0x5C ticks into the current beat */
    unsigned short bar_number;          /* 0x5E FE */
} SeqSong;

/* One sound command, as the game queues it (snd_queue) or runs it at once (snd_run_command). */
typedef struct SoundCommand {
    unsigned short id;
    unsigned char pad_02[0x02];
    long arg[8]; /* numbers or addresses: long is pointer-sized on the PC and still 32 bits on the PS1 */
} SoundCommand;

/* libspu's SpuCommonAttr (layout from Sony's Libref); only the CD fields are used here. */
typedef struct SpuCommonAttr {
    unsigned int mask;
    short master_volume[2];
    short master_volume_mode[2];
    short master_volume_now[2];
    short cd_volume[2];
    int cd_reverb;
    int cd_mix;
    short ext_volume[2];
    int ext_reverb;
    int ext_mix;
} SpuCommonAttr;

/* libspu's SpuReverbAttr (layout from Sony's Libref). */
typedef struct SpuReverbAttr {
    unsigned int mask;
    unsigned int mode;
    short depth[2];
    int delay;
    int feedback;
} SpuReverbAttr;

extern SpuReverbAttr snd_reverb_attr;
void SpuGetReverbModeParam(SpuReverbAttr *attr);
void SpuSetReverbModeParam(SpuReverbAttr *attr);
int SpuSetReverb(int on);

/* INSTR.DAT, copied whole by load_instrument_bank. */
typedef struct Instrument {
    unsigned int sample;                /* SPU address */
    unsigned int loop;                  /* SPU address */
    unsigned char adsr[8];              /* attack, decay, sustain level, sustain, release rates; attack, sustain, release modes */
    unsigned int pitch[12];             /* each semitone of octave 6 */
} Instrument;

extern Instrument instrument_table[128];

extern SeqSong seq_song;
/* A second song block on channels 24-47, its voices limited to seq_song2_voices. What plays there isn't known yet. */
extern SeqSong seq_song2;
extern unsigned int seq_song2_voices;
/* The two blocks were one array in the original: code that touches both reaches the first through the second's
 * address, which GCC only does within one symbol. */
extern SeqSong seq_songs[2];

/* Each LFO waveform: its steps, which the LFOs walk through. */
extern short *lfo_waveforms[];
/* seq_song fields the driver reads and writes as plain variables (as struct members GCC would share the address). */
extern unsigned int seq_song_active_voices;
extern unsigned int seq_song_key_on_voices;
extern unsigned int seq_song_update_voices;
extern unsigned int seq_song_key_off_voices;
extern unsigned int seq_song_layer_voices;
extern unsigned int seq_song_overlap_voices;
extern unsigned int seq_song_flags;
extern unsigned int seq_song_paused_voices;
extern unsigned short seq_song_branch_level;
extern unsigned short seq_song_id;
extern unsigned int seq_song_tempo;
extern int seq_song_tempo_slide_step;
extern unsigned int seq_song_tempo_phase;
extern int seq_song_reverb_depth;
extern int seq_song_reverb_slide_step;
extern unsigned short seq_song_tempo_slide_ticks;
extern unsigned short seq_song_reverb_slide_ticks;
extern unsigned short seq_song_pause_pending;
extern unsigned short seq_song_beats_per_bar;
extern unsigned short seq_song_bar_beat;
extern unsigned short seq_song_beat_ticks;
extern unsigned short seq_song_beat_tick;
extern unsigned short seq_song_bar_number;

/* The effect channels' copies of the song voice masks, and of the noise clock. */
extern unsigned int effect_noise_voices;
extern unsigned int effect_reverb_voices;
extern unsigned int effect_fm_voices;
extern unsigned short effect_noise_clock;
extern unsigned int effect_voices; /* voices sound effects hold */
extern unsigned int effect_key_on_voices;
extern unsigned int effect_update_voices;
extern unsigned int effect_key_off_voices;
extern unsigned int effect_paused_voices; /* effect_voices while 9D has the effects paused */
extern unsigned int effect_tempo; /* << 16, like a song's */
extern unsigned int effect_tempo_phase;
extern SoundCommand snd_request; /* the game fills it and calls snd_submit_request (seq_tick also hands it to 9B for F3) */
int snd_submit_request(void);

extern int seq_stepping_song2; /* when set, layer voices are numbered from 24 */

/* Every channel, music and effects. A music channel's spare voice (F4, F8) keeps its settings in that voice's entry,
 * and an effect hands its voice back to that voice's entry when it ends. */
extern SeqChannel seq_channels[];

/* Every sequence command handler: the channel, its song (or the effects'), and the channel's SPU voice bit. */
void seq_cmd_end(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_noise_on(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_fm_on(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_decay_rate(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_sustain_level(SeqChannel *ch, SeqSong *song, unsigned int voice);

void load_channel_instrument(SeqChannel *ch, unsigned short instrument);

/* Sound commands A0-B7: a playing effect's balance, pan and pitch shift, on a pair of channels. */
void snd_effect_balance(SoundCommand *cmd, SeqChannel *pair);
void snd_effect_balance_slide(SoundCommand *cmd, SeqChannel *pair);
void snd_effect_pan(SoundCommand *cmd, SeqChannel *pair);
void snd_effect_pan_slide(SoundCommand *cmd, SeqChannel *pair);
void snd_effect_pitch_shift(SoundCommand *cmd, SeqChannel *pair);
void snd_effect_pitch_shift_slide(SoundCommand *cmd, SeqChannel *pair);

/* Driver globals the sound commands set (D0/D1, D4/D5, 90, E0, F2, F3). */
extern int snd_tempo_scale; /* D0: a signed byte << 16; its top byte, unsigned, scales both songs' tempo like D4 */
extern int snd_tempo_scale_step;
extern short snd_tempo_scale_ticks;
extern int snd_pitch_scale; /* D4: a signed byte << 16; its top byte, unsigned, scales every pitch (n/128 up, or n/256) */
extern int snd_pitch_scale_step;
extern short snd_pitch_scale_ticks;
extern int snd_muted_voices;
extern unsigned short snd_reverb_balance;
extern short snd_cd_volume_slide_ticks;
extern int snd_cd_volume; /* C8, << 16 */
extern unsigned char seq_end_stub[]; /* where snd_stop_song points every channel */
extern int snd_cd_volume_slide_step;
extern int snd_master_volume; /* C0: 0-127 << 16 */
extern unsigned short snd_master_volume_top; /* its top half, as the voice update reads it */
extern int snd_master_volume_step;
extern short snd_master_volume_ticks;
extern unsigned short snd_reverb_scale;
extern unsigned int snd_flags;
extern unsigned short snd_fade_ticks;
extern unsigned short snd_slow_slide_tick; /* snd_slow_slides runs on every 4th */

void snd_stream_stop(void);
void snd_stop_songs(void);
void snd_stop_effects(void);
void snd_refresh_song_volumes(void);
void snd_refresh_effect_volumes(void);
void snd_stream_start(SoundCommand *cmd);
void snd_stop_effect_pairs(unsigned short pair, int pairs);
void snd_refresh_song_volumes(void);
void snd_refresh_effect_volumes(void);
void func_80039034(SpuCommonAttr *attr); /* probably SpuSetCommonAttr */
void func_80038FEC(void *callback); /* SpuSetTransferCallback (knowledge base §8) */
void func_80038A84(int on);
void func_80038C6C(int on, unsigned int voices);
void SpuSetIRQCallback(void (*callback)(void));
unsigned int SpuSetIRQAddr(unsigned int addr);
void snd_stream_fill_mono_a(void);
extern SpuCommonAttr snd_common_attr;
extern unsigned int song_data[];
extern SoundCommand snd_queue[];
extern void (*snd_command_handlers[256])(SoundCommand *cmd);
extern int snd_queue_locked; /* while set (during snd_submit_request), snd_flush_queue leaves the queue alone */
extern int snd_queue_count;
void snd_effect_streams(unsigned char **first, unsigned char **second, int effect);
extern unsigned short *effect_offsets; /* EFFECT.ALL's table: two stream offsets per effect, 0xFFFF = none */
extern unsigned char *effect_data;     /* what the offsets count from */
void snd_start_effect(unsigned short pan, unsigned short pair, unsigned char *first, unsigned char *second);
void snd_start_lasting_effect(unsigned char *first, unsigned char *second);
void snd_fade_out_song(void);
void snd_load_song(unsigned int *data, unsigned int size); /* copies the song into song_data */
void snd_start_song(void); /* starts the loaded song */
void snd_stop_song(void);
void snd_keep_sounding_voices(void);
void snd_resume_song(int slot);
void snd_save_song(SeqChannel *channels, SeqChannel *saved_channels, SeqSong *song, SeqSong *saved);

/* Two saved songs, for sound commands 10/14: slot 1 holds song 14, slot 0 any other. A slot is empty when its
 * song_id is 0 (F2/F3 empty them). */
extern SeqChannel saved_song_channels[2][24];
extern SeqSong saved_songs[2];
void snd_cmd_10(SoundCommand *cmd);
void snd_cmd_14(SoundCommand *cmd);
void snd_apply_cd_volume(void);
void snd_apply_reverb_voices(void);
void snd_apply_fm_voices(void);
void snd_apply_noise_voices(void);
void snd_key_off_voices(void);
void seq_cmd_noise_off(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_fm_off(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_reverb_off(SeqChannel *ch, SeqSong *song, unsigned int voice);

/* Settings written straight to a voice, not through a channel: 9B/9D's silent hold, snd_stream_voice_mono's voice 16. */
extern VoiceSettings snd_direct_voice;
/* Its fields, which the driver reaches as plain variables. */
extern unsigned int snd_direct_voice_update;
extern unsigned int snd_direct_voice_sample;
extern unsigned int snd_direct_voice_loop;
extern unsigned int snd_direct_voice_attack_mode;
extern unsigned int snd_direct_voice_sustain_mode;
extern unsigned int snd_direct_voice_release_mode;
extern unsigned short snd_direct_voice_pitch;
extern unsigned short snd_direct_voice_attack_rate;
extern unsigned short snd_direct_voice_decay_rate;
extern unsigned short snd_direct_voice_sustain_level;
extern unsigned short snd_direct_voice_sustain_rate;
extern unsigned short snd_direct_voice_release_rate;
extern short snd_direct_voice_volume_left;
extern short snd_direct_voice_volume_right;
void snd_update_voice(unsigned int voice, VoiceSettings *settings);
void snd_stream_fill_stereo_a(void);
void snd_add_spare_voices(SeqChannel *ch, unsigned int *mask, unsigned int voices, unsigned int spares);
void func_80037964(int on, unsigned int voices); /* noise voices */
void func_800388C4(int on, unsigned int voices); /* reverb voices */
void func_80039010(int on, unsigned int voices); /* FM voices */

/* A sample stream from main RAM, double-buffered through SPU 0x77000-0x78FFF on voice 16 (mono) or 16/17 (stereo).
 * Each half-buffer IRQ copies the next 4 KB into the other half. */
extern unsigned int snd_stream_voices;
extern unsigned int snd_stream_left;          /* bytes still to copy, 0 = stopping */
extern unsigned char *snd_stream_pos;
extern unsigned char *snd_stream_loop;        /* where it starts again at the end, 0 = it stops */
extern unsigned int snd_stream_loop_size;
extern int snd_stream_volume;
extern int snd_stream_pan;                    /* mono: 0 left to 127 right */
extern unsigned short snd_stream_pitch;
void func_80038F64(unsigned int addr); /* SpuSetTransferStartAddr */
void func_80038F04(unsigned char *data, unsigned int size); /* SpuWrite */
void func_800396C0(unsigned int voice, unsigned int addr); /* a voice's loop address */
void snd_stream_fill_mono_b(void);
void snd_stream_fill_stereo_b(void);

void snd_keep_looping_effects(void);
extern unsigned char seq_cmd_sizes[256]; /* each command's size in bytes with its arguments; 0 for the ones that end a stream */
extern unsigned char seq_cmd_scan_sizes[256]; /* the same, but 0 for the loop and jump commands seq_look_ahead follows */
extern void (*seq_cmd_handlers[96])(SeqChannel *ch, SeqSong *song, unsigned int voice); /* commands A0-FF */
extern unsigned short seq_note_lengths[11]; /* a note's gate for each of its 11 lengths: key-off << 8 | ticks */
unsigned char seq_look_ahead(SeqChannel *ch);
unsigned char seq_stream_end(SeqChannel *ch); /* skips to the stream's last command: CA (loops forever) or A0 (ends) */
int SpuGetKeyStatus(unsigned int voice);
int GetRCnt(unsigned int counter);
int SetRCnt(unsigned int counter, unsigned short target, int mode);
int StartRCnt(unsigned int counter);
int StopRCnt(unsigned int counter);
int OpenEvent(unsigned int desc, int spec, int mode, unsigned int (*handler)(void));
int EnableEvent(int event);
int DisableEvent(int event);
int CloseEvent(int event);
void UnDeliverEvent(unsigned int desc, int spec);
void SpuInitMalloc(int count, unsigned char *table);
int SpuMallocWithStartAddr(unsigned int addr, int size);
void func_80037BE0(unsigned char *data, unsigned int size); /* SpuRead */
void func_80039450(int voice, int left, int right, int left_mode, int right_mode); /* a voice's volume */
unsigned int snd_timer_tick(void);
void load_instrument_bank(unsigned int *header, unsigned int *instruments);
void load_instrument_bank2(unsigned int *header, unsigned int *instruments);
extern unsigned char snd_spu_malloc_table[]; /* SpuInitMalloc's */
extern unsigned int effect_file[];           /* EFFECT.ALL: the offsets, then from 0x1000 the data */
extern unsigned char snd_silent_blocks[32];  /* written to SPU 0x76FE0: the sample F2 and stopped streams point at */
int VSync(int mode);
void seq_tick(void);
void seq_step_channel(SeqChannel *ch, SeqSong *song, unsigned int voice);
extern unsigned short snd_timer_last_count; /* VSync(1) at the last snd_timer_tick */
extern int snd_tick_time[4];
extern int snd_tick_time_total;

/* A fading channel of the second block (18/19): volume << 16, its step per tick, and the ticks left. */
typedef struct ChannelFade {
    int volume;
    int step;
    unsigned short ticks;
} ChannelFade;
extern ChannelFade snd_channel_fades[24];

void func_800395C8(unsigned int voice, unsigned short pitch); /* libspu voice setters: pitch, volume, start address */
void func_800393C8(unsigned int voice, short left, short right);
void func_80039644(unsigned int voice, unsigned int addr);
void SpuSetVoiceSRAttr(unsigned int voice, unsigned short rate, unsigned int mode);
void SpuSetVoiceARAttr(unsigned int voice, unsigned short rate, unsigned int mode);
void SpuSetVoiceRRAttr(unsigned int voice, unsigned short rate, unsigned int mode);
void SpuSetVoiceDR(unsigned int voice, unsigned short rate);
void SpuSetVoiceSL(unsigned int voice, unsigned short level);

extern unsigned int snd_stream_flags; /* the header's flags; snd_stream_pitch is its top half */
void func_80038FB8(int mode);
void snd_stream_play_mono(void);
void snd_stream_voice_mono(void);
void snd_stream_voices_stereo(void);
void snd_stream_play_stereo(void);

/* The pan tables (by pan 0-255): each side's share, out of 32768. */
extern short snd_pan_left[256];
extern short snd_pan_right[256];

void seq_channel_lfo(SeqChannel *ch, unsigned int voice, unsigned short index);
void seq_effect_lfo(SeqChannel *ch, unsigned int voice);
void seq_layer_voice_update(SeqChannel *ch, unsigned int voices, unsigned int voice);

void SpuSetReverbDepth(SpuReverbAttr *attr);
void SpuSetNoiseClock(int clock);

#endif
