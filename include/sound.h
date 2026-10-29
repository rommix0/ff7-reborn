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

/* SeqSong.flags */
#define SONG_UPDATE_NOISE  0x10 /* the noise voices or clock changed */
#define SONG_UPDATE_REVERB 0x80 /* the reverb depth changed */

/* SeqChannel.voice_update: which SPU voice settings snd_update_voice writes next (libspu's voice attribute mask) */
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
    unsigned char pad_34[0x04];
    unsigned int flags;                 /* 0x38 CHANNEL_* */
    int pitch_shift;                    /* 0x3C set by the game on a playing effect: a signed byte << 8 */
    int pitch_shift_step;               /* 0x40 */
    int volume;                         /* 0x44 A8 value << 23 */
    int volume_slide_step;              /* 0x48 */
    unsigned char pad_4C[0x08];
    unsigned short is_effect;           /* 0x54 a sound effect's channel */
    unsigned short gate;                /* 0x56 key-off countdown << 8 | ticks to the next event */
    unsigned short instrument;          /* 0x58 */
    unsigned short pitch_shift_slide_ticks; /* 0x5A */
    unsigned short volume_slide_ticks;  /* 0x5C */
    unsigned short balance_slide_ticks; /* 0x5E */
    unsigned short pan;                 /* 0x60 AA value << 8 */
    unsigned short pan_slide_ticks;     /* 0x62 */
    unsigned char pad_64[0x02];
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
    unsigned short fixed_length;        /* 0xC4 DC, 0 = off */
    short balance;                      /* 0xC6 F6 value << 8: the layer voice's share */
    short balance_slide_step;           /* 0xC8 */
    short pan_slide_step;               /* 0xCA */
    short transpose;                    /* 0xCC */
    short detune;                       /* 0xCE */
    unsigned char pad_D0[0x02];
    short bend_semitones;               /* 0xD2 A4, spent by the next note or tie */
    short last_transpose;               /* 0xD4 the transpose when the last note played (a glide starts from it) */
    short vibrato_offset;               /* 0xD6 */
    short tremolo_offset;               /* 0xD8 */
    short pan_lfo_offset;               /* 0xDA */
    unsigned char pad_DC[0x04];
    unsigned int voice_update;          /* 0xE0 UPDATE_* */
    unsigned int sample;                /* 0xE4 the instrument's SPU sample address */
    unsigned int loop;                  /* 0xE8 and loop address */
    unsigned int attack_mode;           /* 0xEC B7 */
    unsigned int sustain_mode;          /* 0xF0 BB */
    unsigned int release_mode;          /* 0xF4 BF */
    unsigned short pitch;               /* 0xF8 the last pitch register written */
    unsigned short attack_rate;         /* 0xFA AD */
    unsigned short decay_rate;          /* 0xFC AE */
    unsigned short sustain_level;       /* 0xFE AF */
    unsigned short sustain_rate;        /* 0x100 B1 */
    unsigned short release_rate;        /* 0x102 B2 */
    unsigned char pad_104[0x04];
} SeqChannel;

/* The song or effect that owns the channels, handed to every command handler. */
typedef struct SeqSong {
    unsigned int unk_00;                /* 0x00 sound commands 80/81/82 set 1/2/4 */
    unsigned int active_voices;         /* 0x04 the song's own channels */
    unsigned int voices_08;             /* 0x08 a voice mask (A0 on an effect clears its bit) */
    unsigned int update_voices;         /* 0x0C voices the driver keeps updating (key-off takes a voice out) */
    unsigned int voices_10;             /* 0x10 a voice mask (likewise) */
    unsigned char pad_14[0x04];
    unsigned int tempo;                 /* 0x18 E8 value << 16 */
    int tempo_slide_step;               /* 0x1C E9 */
    unsigned char pad_20[0x04];
    unsigned int layer_voices;          /* 0x24 voices held by F4 layers */
    unsigned int overlap_voices;        /* 0x28 voices held by F8 overlaps */
    unsigned int noise_voices;          /* 0x2C C4/C5 */
    unsigned int reverb_voices;         /* 0x30 C2/C3 */
    unsigned int fm_voices;             /* 0x34 C6/C7 */
    unsigned int flags;                 /* 0x38 SONG_* */
    unsigned char pad_3C[0x04];
    int reverb_depth;                   /* 0x40 EA value << 16 */
    int reverb_slide_step;              /* 0x44 EB */
    unsigned short tempo_slide_ticks;   /* 0x48 */
    unsigned short song_id;             /* 0x4A the playing song (sound command 10); A0 clears it when the last channel ends */
    unsigned short branch_taken;        /* 0x4C EF: the n of the last branch taken */
    unsigned short branch_level;        /* 0x4E EF branches when this is non-zero and at least n (set by the game) */
    unsigned short reverb_slide_ticks;  /* 0x50 */
    unsigned short noise_clock;         /* 0x52 AC */
    unsigned short unk_54;              /* 0x54 F3 sets it to 1 */
    unsigned short beats_per_bar;       /* 0x56 FD */
    unsigned short beat_tick;           /* 0x58 FD zeroes it (a counter) */
    unsigned short beat_ticks;          /* 0x5A FD ticks per beat */
    unsigned short bar_beat;            /* 0x5C FD zeroes it (a counter) */
    unsigned short bar_number;          /* 0x5E FE */
} SeqSong;

/* One sound command, as the game queues it (snd_queue) or runs it at once (snd_run_command). */
typedef struct SoundCommand {
    unsigned short id;
    unsigned char pad_02[0x02];
    int arg[8];
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

/* INSTR.DAT, copied whole by load_instrument_bank. */
typedef struct Instrument {
    unsigned int sample;                /* SPU address */
    unsigned int loop;                  /* SPU address */
    unsigned char adsr[8];              /* attack, decay, sustain level, sustain, release rates; attack, sustain, release modes */
    unsigned int pitch[12];             /* each semitone of octave 6 */
} Instrument;

extern Instrument instrument_table[128];

extern SeqSong seq_song;

/* Each LFO waveform: its steps, which the LFOs walk through. */
extern short *lfo_waveforms[];
/* seq_song fields the driver reads and writes as plain variables (as struct members GCC would share the address). */
extern unsigned int seq_song_voices_08;
extern unsigned int seq_song_update_voices;
extern unsigned int seq_song_voices_10;
extern unsigned int seq_song_flags;
extern unsigned short seq_song_branch_level;
extern unsigned short seq_song_id;

/* The effect channels' copies of the song voice masks, and of the noise clock. */
extern unsigned int effect_noise_voices;
extern unsigned int effect_reverb_voices;
extern unsigned int effect_fm_voices;
extern unsigned short effect_noise_clock;
extern unsigned int effect_voices; /* voices sound effects hold */

extern int D_80062F04; /* when set, layer voices are numbered from 24 */

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

/* Driver globals the sound commands set, not yet understood (D0/D1, D4/D5, 90, E0, F2, F3). */
extern int D_80062FE8;
extern int D_80062F30;
extern unsigned short D_80062F48;
extern int D_80062FE4;
extern int D_80062F28;
extern unsigned short D_80062F40;
extern int D_80062FD8;
extern unsigned short D_80062F70;
extern unsigned short snd_cd_volume_slide_ticks;
extern int snd_cd_volume; /* C8, << 16 */
extern unsigned int D_80062F00;
extern int snd_cd_volume_slide_step;
extern int D_80062F5C;
extern int D_80062F2C;
extern unsigned short D_80062F44;
extern unsigned short D_80062FB8;
extern unsigned int D_80062FF8;
extern unsigned short D_80062FC8;

void func_80029A50(void);
void func_80029F44(void);
void func_8002A43C(void);
void snd_refresh_song_volumes(void);
void snd_refresh_effect_volumes(void);
void func_8002CFC0(SoundCommand *cmd);
void func_8002A510(int pair, int arg);
void snd_refresh_song_volumes(void);
void snd_refresh_effect_volumes(void);
void func_80039034(SpuCommonAttr *attr); /* probably SpuSetCommonAttr */
extern SpuCommonAttr snd_common_attr;
extern unsigned int song_data[];
extern SoundCommand snd_queue[];
extern int snd_queue_count;
void func_8002A6C4(unsigned char **first, unsigned char **second, int effect); /* an effect's two streams */
void func_8002A094(int pan, int pair, unsigned char *first, unsigned char *second); /* starts an effect */
void func_8002A28C(unsigned char *first, unsigned char *second);
void func_8002AFB8(void);
void snd_load_song(unsigned int *data, unsigned int size); /* copies the song into song_data */
void func_80029C48(void); /* starts the loaded song */
void func_80029E98(void);
void func_8002A7E8(void);
void snd_resume_song(int slot);
void snd_save_song(SeqChannel *channels, SeqChannel *saved_channels, SeqSong *song, SeqSong *saved);

/* Two saved songs, for sound commands 10/14: slot 1 holds song 14, slot 0 any other. A slot is empty when its
 * song_id is 0 (F2/F3 empty them). */
extern SeqChannel saved_song_channels[2][24];
extern SeqSong saved_songs[2];
void snd_cmd_10(SoundCommand *cmd);
void snd_cmd_14(SoundCommand *cmd);
void snd_apply_cd_volume(void);
void func_80030038(void); /* hands the reverb voice masks to the SPU (by analogy with func_80030148) */
void func_80030148(void); /* hands the FM voice masks to the SPU */
void func_8002FF4C(void); /* hands the noise voice masks to the SPU (by analogy) */
void seq_cmd_noise_off(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_fm_off(SeqChannel *ch, SeqSong *song, unsigned int voice);
void seq_cmd_reverb_off(SeqChannel *ch, SeqSong *song, unsigned int voice);

#endif
