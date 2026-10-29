/* With -G8, GCC writes every function after the file's top-level asm, which would put INCLUDE_ASM's functions first.
 * maspsx's workaround: each include sits inside a function whose wrapper it strips (its README). */
#define INCLUDE_ASM(FOLDER, NAME) \
    void __maspsx_include_asm_hack_##NAME() { \
        __asm__( \
            ".text # maspsx-keep \n" \
            "\t.align\t2 # maspsx-keep\n" \
            "\t.set noreorder # maspsx-keep\n" \
            "\t.set noat # maspsx-keep\n" \
            ".include \""FOLDER"/"#NAME".s\" # maspsx-keep\n" \
            "\t.set reorder # maspsx-keep\n" \
            "\t.set at # maspsx-keep\n" \
        ); \
    }
#include "common.h"
#include "sound.h"

/* This part of the driver was compiled with -G8, so the small globals it uses are reached through $gp. GCC and maspsx
 * only do that for variables the file defines: these are tentative definitions, left common, which the linker
 * resolves to the data section's own labels. */
volatile unsigned short snd_transfer_busy;
int snd_timer_event;
unsigned short *effect_offsets;
unsigned char *effect_data;
int snd_master_volume;
int snd_cd_volume;
unsigned int D_80062F68;
unsigned int snd_stream_voices;
short snd_cd_volume_slide_ticks;
short snd_master_volume_ticks;
short snd_pitch_scale_ticks;
int snd_pitch_scale;
short snd_tempo_scale_ticks;
int snd_tempo_scale;
unsigned short snd_reverb_scale;
unsigned short snd_timer_last_count;
int snd_queue_locked;
int snd_queue_count;
int snd_muted_voices;
unsigned int D_80062FF8;
unsigned short snd_reverb_balance;


/* SPU transfers: one at a time, snd_transfer_busy set until the transfer callback clears it. */
void snd_transfer_done(void)
{
    func_80038FEC(0);
    snd_transfer_busy = 0;
}

void snd_transfer_begin(void)
{
    snd_transfer_busy = 1;
    func_80038FEC(snd_transfer_done);
}

void snd_transfer_write(unsigned char *data, unsigned int size)
{
    snd_transfer_begin();
    func_80038F04(data, size);
}

void snd_transfer_read(unsigned char *data, unsigned int size)
{
    snd_transfer_begin();
    func_80037BE0(data, size);
}

void snd_transfer_wait(void)
{
    do
        ;
    while (snd_transfer_busy);
}

/* Puts the driver in its boot state: no songs or effects, full volumes, the SPU's common attributes, every channel
 * on its own voice and the effect pairs (48-55) centred on voices 16-23. */
void snd_reset(void)
{
    unsigned short i;
    SeqChannel *ch;
    ChannelFade *fade;

    seq_song.unk_00 = 1;
    snd_master_volume = 0x7F0000;
    snd_cd_volume = 0x7FFF0000;
    saved_songs[1].song_id = 0;
    saved_songs[0].song_id = 0;
    seq_song2.song_id = 0;
    seq_song_id = 0;
    saved_songs[1].active_voices = 0;
    saved_songs[0].active_voices = 0;
    effect_voices = 0;
    seq_song2.active_voices = 0;
    seq_song_active_voices = 0;
    D_80062F68 = 0;
    snd_stream_voices = 0;
    effect_paused_voices = 0;
    seq_song_paused_voices = 0;
    snd_cd_volume_slide_ticks = 0;
    snd_master_volume_ticks = 0;
    snd_pitch_scale_ticks = 0;
    snd_pitch_scale = 0;
    snd_tempo_scale_ticks = 0;
    snd_tempo_scale = 0;
    effect_fm_voices = 0;
    seq_song.fm_voices = 0;
    effect_reverb_voices = 0;
    seq_song.reverb_voices = 0;
    effect_noise_voices = 0;
    seq_song.noise_voices = 0;
    snd_queue[0].id = 0;
    seq_song_beat_ticks = 0;
    seq_song_bar_beat = 0;
    seq_song_beats_per_bar = 0;
    seq_song_bar_number = 0;
    seq_song_reverb_slide_ticks = 0;
    seq_song.reverb_depth = 0;
    seq_song.reverb_mode = 0;
    snd_common_attr.mask = 0x3FCF;
    snd_common_attr.master_volume[1] = 0x3FFF;
    snd_common_attr.master_volume[0] = 0x3FFF;
    snd_common_attr.master_volume_mode[0] = 0;
    snd_common_attr.master_volume_mode[1] = 0;
    snd_common_attr.cd_volume[1] = 0x7FFF;
    snd_common_attr.cd_volume[0] = 0x7FFF;
    snd_common_attr.cd_reverb = 0;
    snd_common_attr.cd_mix = 1;
    snd_common_attr.ext_volume[1] = 0;
    snd_common_attr.ext_volume[0] = 0;
    snd_common_attr.ext_reverb = 0;
    snd_common_attr.ext_mix = 0;
    func_80039034(&snd_common_attr);
    snd_reverb_scale = 0;
    snd_timer_last_count = 0;
    snd_queue_locked = 0;
    snd_queue_count = 0;
    snd_muted_voices = 0;
    D_80062F68 = 0;
    D_80062FF8 = 0;
    snd_reverb_balance = 0x40;
    fade = snd_channel_fades;
    ch = seq_channels;
    for (i = 0; i < 24; i++, ch++, fade++) {
        fade->ticks = 0;
        fade->step = 0;
        ch->unk_50 = 0;
        ch->flags = 0;
        ch->voice.voice = i;
        ch->is_effect = 0;
        func_80039450(i, 0, 0, 0, 0);
        fade->volume = 0x7F0000;
    }
    ch = &seq_channels[48];
    for (i = 16; i < 24; i++, ch++) {
        ch->pitch_shift_slide_ticks = 0;
        ch->pitch_shift = 0;
        ch->balance_slide_ticks = 0;
        ch->unk_50 = 0;
        ch->flags = 0;
        ch->overlap_voice = i;
        ch->voice.voice = i;
        ch->is_effect = 1;
        ch->balance = 0x7F00;
    }
    effect_tempo_phase = 1;
    effect_key_off_voices = 0;
    effect_update_voices = 0;
    effect_key_on_voices = 0;
    seq_song_key_off_voices = 0;
    seq_song_update_voices = 0;
    seq_song_key_on_voices = 0;
    effect_tempo = 0x66A80000;
}

/* The instrument banks: the samples go to SPU RAM (the header's address and size, then the data) and the table to
 * instrument_table (all 128, or from 53 for the second bank). */
INCLUDE_ASM("asm/nonmatchings/main/sound_init", load_instrument_bank);

INCLUDE_ASM("asm/nonmatchings/main/sound_init", load_instrument_bank2);

/* Boots the driver: the effect file's buffer, SPU memory from 0x77000 for the stream, the first bank, 32 silent
 * bytes at 0x76FE0, a reset, then root counter 2 at 0x43D1 with snd_timer_tick on its interrupt. */
void snd_init(unsigned int *header, unsigned int *instruments)
{
    effect_offsets = (unsigned short *)effect_file;
    effect_data = (unsigned char *)effect_file + 0x1000;
    SpuInitMalloc(4, snd_spu_malloc_table);
    SpuMallocWithStartAddr(0x77000, 0x2000);
    func_80038FB8(0);
    load_instrument_bank(header, instruments);
    func_80038F64(0x76FE0);
    snd_transfer_write(snd_silent_blocks, 0x20);
    snd_transfer_wait();
    snd_reset();
    while ((snd_timer_event = OpenEvent(0xF2000002, 2, 0x1000, snd_timer_tick)) == -1)
        ;
    while (!EnableEvent(snd_timer_event))
        ;
    while (!SetRCnt(0xF2000002, 0x43D1, 0x1000))
        ;
    while (!StartRCnt(0xF2000002))
        ;
}

/* Copies EFFECT.ALL (0xC800 bytes) into its buffer. */
void snd_load_effects(unsigned int *table)
{
    unsigned int *to = (unsigned int *)effect_offsets;
    unsigned short n;

    n = 0x3200;
    do {
        n--;
        *to++ = *table++;
    } while (n);
}

/* Stops the timer interrupt and the SPU callbacks and keys every voice off. */
void snd_quit(void)
{
    while (!StopRCnt(0xF2000002))
        ;
    UnDeliverEvent(0xF2000002, 2);
    while (!DisableEvent(snd_timer_event))
        ;
    while (!CloseEvent(snd_timer_event))
        ;
    func_80038FEC(0);
    func_80038A84(0);
    SpuSetIRQCallback(0);
    func_80038C6C(0, 0xFFFFFF);
}
