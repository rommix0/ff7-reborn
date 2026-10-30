/* Runs the sound driver on the PC with libspu and the kernel stubbed out: plays one song for a number of sequencer
 * ticks and prints each tick's key-ons. tools/native_check.py compares them with tools/akao.py.
 *
 * usage: snd_native SCUS_941.63 INSTR.ALL INSTR.DAT SONG TICKS   (SONG: one AKAO block)
 *
 * The driver's data lives in snd_ram, a copy of the game's RAM loaded from the user's own executable: the linker
 * script build/native/ram.ld (tools/native_syms.py) places every data symbol at its game address in it. Only the
 * objects that hold pointers are real C definitions here, since a PC pointer doesn't fit the game's 4 bytes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "sound.h"

#define RAM_BASE 0x80000000u

unsigned char snd_ram[0x200000];

SeqChannel seq_channels[56];
SeqChannel saved_song_channels[2][24];
SoundCommand snd_queue[128];
SoundCommand snd_request;
void (*snd_command_handlers[256])(SoundCommand *cmd);
void (*seq_cmd_handlers[96])(SeqChannel *ch, SeqSong *song, unsigned int voice);
short *lfo_waveforms[16];
unsigned short *effect_offsets;
unsigned char *effect_data;
unsigned char *snd_stream_pos;
unsigned char *snd_stream_loop;

/* build/native/funcs.c: each driver function's game address. */
struct GameFunc {
    unsigned int addr;
    void *func;
};
extern const struct GameFunc game_funcs[];
extern const int game_func_count;

void seq_tick(void);
int snd_submit_request(void);
void snd_init(unsigned int *header, unsigned int *instruments);

#define RAM(addr) (snd_ram + ((addr) - RAM_BASE))

static unsigned int word_at(const unsigned char *p)
{
    unsigned int w;

    memcpy(&w, p, 4);
    return w;
}

/* A code address from one of the game's tables, as the native function. */
static void *game_func(unsigned int addr)
{
    int i;

    for (i = 0; i < game_func_count; i++)
        if (game_funcs[i].addr == addr)
            return game_funcs[i].func;
    fprintf(stderr, "no native function for %08X\n", addr);
    exit(1);
}

static unsigned char *read_file(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    unsigned char *data;

    if (!f) {
        perror(path);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    rewind(f);
    data = malloc(*size);
    if (fread(data, 1, *size, f) != (size_t)*size) {
        perror(path);
        exit(1);
    }
    fclose(f);
    return data;
}

/* The tick of the last sequencer step, which the next pass's key-ons belong to. */
static int last_tick = -1;
static unsigned int keyed_voices;

/* libspu and the kernel: only what the driver's flow depends on (transfers finish at once, setup succeeds). */
static void (*transfer_callback)(void);
void func_80038FEC(void *callback) { transfer_callback = callback; }
void func_80038F04(unsigned char *data, unsigned int size) { (void)data; (void)size; if (transfer_callback) transfer_callback(); }
void func_80037BE0(unsigned char *data, unsigned int size) { (void)data; (void)size; if (transfer_callback) transfer_callback(); }
void hw_poll(void) {} /* transfers finish at once, so a wait has nothing to give a turn to */
/* Key-ons print by song channel, a channel's F4/F8 spare voice counting as its own: akao.py reports notes that way. */
void func_80038C6C(int on, unsigned int voices)
{
    unsigned int channels = voices & seq_song_active_voices;
    SeqChannel *ch;
    int i;

    if (!on) {
        keyed_voices &= ~voices;
        return;
    }
    keyed_voices |= voices;
    for (i = 0; i < 24; i++) {
        ch = &seq_channels[i];
        if (!(seq_song_active_voices >> i & 1))
            continue;
        if ((ch->flags & CHANNEL_OVERLAP && voices >> ch->overlap_voice & 1)
            || (ch->flags & CHANNEL_LAYER && voices >> ch->layer_voice & 1))
            channels |= 1u << i;
    }
    printf("%d %06X\n", last_tick, channels);
}
int SpuGetKeyStatus(unsigned int voice) { return keyed_voices & voice ? 1 : 0; }
void func_80038F64(unsigned int addr) { (void)addr; }
void func_80038FB8(int mode) { (void)mode; }
void func_80038A84(int on) { (void)on; }
void func_80039034(SpuCommonAttr *attr) { (void)attr; }
void func_80037964(int on, unsigned int voices) { (void)on; (void)voices; }
void func_800388C4(int on, unsigned int voices) { (void)on; (void)voices; }
void func_80039010(int on, unsigned int voices) { (void)on; (void)voices; }
void func_800393C8(unsigned int voice, short left, short right) { (void)voice; (void)left; (void)right; }
void func_80039450(int voice, int left, int right, int left_mode, int right_mode) { (void)voice; (void)left; (void)right; (void)left_mode; (void)right_mode; }
void func_800395C8(unsigned int voice, unsigned short pitch) { (void)voice; (void)pitch; }
void func_80039644(unsigned int voice, unsigned int addr) { (void)voice; (void)addr; }
void func_800396C0(unsigned int voice, unsigned int addr) { (void)voice; (void)addr; }
void SpuSetVoiceARAttr(unsigned int voice, unsigned short rate, unsigned int mode) { (void)voice; (void)rate; (void)mode; }
void SpuSetVoiceSRAttr(unsigned int voice, unsigned short rate, unsigned int mode) { (void)voice; (void)rate; (void)mode; }
void SpuSetVoiceRRAttr(unsigned int voice, unsigned short rate, unsigned int mode) { (void)voice; (void)rate; (void)mode; }
void SpuSetVoiceDR(unsigned int voice, unsigned short rate) { (void)voice; (void)rate; }
void SpuSetVoiceSL(unsigned int voice, unsigned short level) { (void)voice; (void)level; }
void SpuGetReverbModeParam(SpuReverbAttr *attr) { (void)attr; }
void SpuSetReverbModeParam(SpuReverbAttr *attr) { (void)attr; }
void SpuSetReverbDepth(SpuReverbAttr *attr) { (void)attr; }
int SpuSetReverb(int on) { return on; }
void SpuSetNoiseClock(int clock) { (void)clock; }
void SpuSetIRQCallback(void (*callback)(void)) { (void)callback; }
unsigned int SpuSetIRQAddr(unsigned int addr) { return addr; }
void SpuInitMalloc(int count, unsigned char *table) { (void)count; (void)table; }
int SpuMallocWithStartAddr(unsigned int addr, int size) { (void)size; return addr; }
int OpenEvent(unsigned int desc, int spec, int mode, unsigned int (*handler)(void)) { (void)desc; (void)spec; (void)mode; (void)handler; return 1; }
int EnableEvent(int event) { (void)event; return 1; }
int DisableEvent(int event) { (void)event; return 1; }
int CloseEvent(int event) { (void)event; return 1; }
void UnDeliverEvent(unsigned int desc, int spec) { (void)desc; (void)spec; }
int SetRCnt(unsigned int counter, unsigned short target, int mode) { (void)counter; (void)target; (void)mode; return 1; }
int StartRCnt(unsigned int counter) { (void)counter; return 1; }
int StopRCnt(unsigned int counter) { (void)counter; return 1; }
int GetRCnt(unsigned int counter) { (void)counter; return 0; }
int VSync(int mode) { (void)mode; return 0; }

int main(int argc, char **argv)
{
    unsigned char *exe, *instr_all, *instr_dat, *song;
    long size;
    int i, ticks, tick = 0;
    int due;

    if (argc != 6) {
        fprintf(stderr, "usage: snd_native SCUS_941.63 INSTR.ALL INSTR.DAT SONG TICKS\n");
        return 2;
    }
    /* A PS-X EXE: the load address and size at 0x18, the code and data from 0x800. */
    exe = read_file(argv[1], &size);
    memcpy(RAM(word_at(exe + 0x18)), exe + 0x800, word_at(exe + 0x1C));
    /* The tables' game addresses: snd_command_handlers, seq_cmd_handlers, lfo_waveforms. */
    for (i = 0; i < 256; i++)
        snd_command_handlers[i] = game_func(word_at(RAM(0x80049548 + i * 4)));
    for (i = 0; i < 96; i++)
        seq_cmd_handlers[i] = game_func(word_at(RAM(0x80049AA8 + i * 4)));
    for (i = 0; i < 16; i++)
        lfo_waveforms[i] = (short *)RAM(word_at(RAM(0x8004A5CC + i * 4)));

    instr_all = read_file(argv[2], &size);
    instr_dat = read_file(argv[3], &size);
    song = read_file(argv[4], &size);
    ticks = atoi(argv[5]);
    snd_init((unsigned int *)instr_all, (unsigned int *)instr_dat);

    snd_request.id = 0x10;
    snd_request.arg[0] = (long)song;
    snd_submit_request();
    while (tick < ticks) {
        /* seq_tick steps the song when the tempo carries out of the phase's low half (no D0 scale here). */
        due = seq_song_active_voices && (seq_song_tempo >> 16) + seq_song_tempo_phase > 0xFFFF;
        seq_tick();
        if (due)
            last_tick = tick++;
    }
    return 0;
}
