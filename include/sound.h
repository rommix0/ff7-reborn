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

/* SeqSong.flags */
#define SONG_UPDATE_REVERB 0x80 /* the reverb depth changed */

/* SeqChannel.voice_update: which SPU voice settings snd_update_voice writes next (libspu's voice attribute mask) */
#define UPDATE_VOLUME        0x3 /* both sides */
#define UPDATE_PITCH         0x10
#define UPDATE_ATTACK_MODE   0x100
#define UPDATE_SUSTAIN_MODE  0x200
#define UPDATE_RELEASE_MODE  0x400
#define UPDATE_ATTACK_RATE   0x800
#define UPDATE_DECAY_RATE    0x1000
#define UPDATE_SUSTAIN_RATE  0x2000
#define UPDATE_RELEASE_RATE  0x4000
#define UPDATE_SUSTAIN_LEVEL 0x8000

/* One sequence channel of the AKAO driver, 0x108 bytes (docs/knowledge_base.md §4). Fields get names as the
 * functions that use them are recovered; the pads are still unread. */
typedef struct SeqChannel {
    unsigned char *pos;                 /* 0x00 read pointer into the sequence */
    unsigned char *loop_start[4];       /* 0x04 C8, one per loop level */
    unsigned char *drum_map;            /* 0x14 EC: 5 bytes per key */
    unsigned char pad_18[0x0C];
    unsigned int layer_voice;           /* 0x24 F4: the SPU voice of the second voice */
    unsigned char pad_28[0x04];
    unsigned int volume_scale;          /* 0x2C A3 */
    unsigned char pad_30[0x08];
    unsigned int flags;                 /* 0x38 CHANNEL_* */
    unsigned char pad_3C[0x08];
    int volume;                         /* 0x44 A8 value << 23 */
    unsigned char pad_48[0x0E];
    unsigned short gate;                /* 0x56 key-off countdown << 8 | ticks to the next event */
    unsigned char pad_58[0x04];
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
    unsigned char pad_70[0x20];
    unsigned short tremolo_depth;       /* 0x90 B9 value << 8 */
    unsigned char pad_92[0x0C];
    unsigned short pan_lfo_depth;       /* 0x9E BD value << 7 */
    unsigned char pad_A0[0x04];
    unsigned short noise_ticks;         /* 0xA4 CE/CF: ticks until noise flips, 0 = off */
    unsigned short fm_ticks;            /* 0xA6 D2/D3: ticks until FM flips, 0 = off */
    unsigned char pad_A8[0x10];
    unsigned short loop_level;          /* 0xB8 0-3 */
    unsigned short loop_count[4];       /* 0xBA passes made at each level */
    unsigned short last_length;         /* 0xC2 the length DC counts from (A2 sets it) */
    unsigned short fixed_length;        /* 0xC4 DC, 0 = off */
    unsigned short balance;             /* 0xC6 F6 value << 8: the layer voice's share */
    unsigned char pad_C8[0x04];
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
    unsigned char pad_E4[0x08];
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
    unsigned char pad_00[0x18];
    unsigned int tempo;                 /* 0x18 E8 value << 16 */
    unsigned char pad_1C[0x1C];
    unsigned int flags;                 /* 0x38 SONG_* */
    unsigned char pad_3C[0x04];
    int reverb_depth;                   /* 0x40 EA value << 16 */
    unsigned char pad_44[0x04];
    unsigned short tempo_slide_ticks;   /* 0x48 */
    unsigned char pad_4A[0x06];
    unsigned short reverb_slide_ticks;  /* 0x50 */
    unsigned short noise_clock;         /* 0x52 AC */
    unsigned short unk_54;              /* 0x54 F3 sets it to 1 */
    unsigned short beats_per_bar;       /* 0x56 FD */
    unsigned short beat_tick;           /* 0x58 FD zeroes it (a counter) */
    unsigned short beat_ticks;          /* 0x5A FD ticks per beat */
    unsigned short bar_beat;            /* 0x5C FD zeroes it (a counter) */
    unsigned short bar_number;          /* 0x5E FE */
} SeqSong;

/* One per SPU voice: effects play on them, and a layer's second voice keeps its settings in its voice's entry. */
extern SeqChannel effect_channels[24];

void seq_cmd_end(SeqChannel *ch);
void seq_cmd_noise_on(SeqChannel *ch, SeqSong *song);
void seq_cmd_fm_on(SeqChannel *ch, SeqSong *song);

#endif
