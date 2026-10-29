#include "common.h"
#include "sound.h"

INCLUDE_ASM("asm/nonmatchings/main/sound", func_800293D0);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_800293F4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029424);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029464);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_800294A4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_800294BC);

INCLUDE_ASM("asm/nonmatchings/main/sound", load_instrument_bank);

INCLUDE_ASM("asm/nonmatchings/main/sound", load_instrument_bank2);

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_init);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029998);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_800299C8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029A50);

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_set_reverb_mode);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029B78);

INCLUDE_ASM("asm/nonmatchings/main/sound", channel_reset);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029C48);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029E98);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80029F44);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A094);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A28C);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A43C);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A510);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A6C4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A748);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A798);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A7E8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A958);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002AABC);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002AFB8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B1A8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B1F8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B2F8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B3B4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B5A8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B608);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B668);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B6AC);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B730);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B7E0);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B8B4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B904);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B958);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002B9AC);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002BA08);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002BA5C);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002BA98);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002BB20);

void snd_cmd_C8(SoundCommand *cmd)
{
    D_80062FD4 = (unsigned short)cmd->arg[0] << 16;
    D_80062FCC = 0;
    func_8002E428();
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002BBEC);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002BC58);

/* Sound commands A0-B7 set or slide the balance, pan or pitch shift of a playing effect, on both channels of its
 * pair (seq_channels 48-55). A slide's length 0 means 1 tick. */
void snd_effect_balance(SoundCommand *cmd, SeqChannel *pair)
{
    short value = (cmd->arg[0] & 0x7F) << 8;

    pair[1].balance_slide_ticks = 0;
    pair[0].balance_slide_ticks = 0;
    pair[1].balance = value;
    pair[0].balance = value;
    pair[0].voice_update |= UPDATE_VOLUME;
    pair[1].voice_update |= UPDATE_VOLUME;
}

void snd_effect_balance_slide(SoundCommand *cmd, SeqChannel *pair)
{
    unsigned short ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    pair[0].balance_slide_step = (short)(((cmd->arg[1] & 0x7F) << 8) - pair[0].balance) / (short)ticks;
    pair[1].balance_slide_step = (short)(((cmd->arg[1] & 0x7F) << 8) - pair[1].balance) / (short)ticks;
    pair[1].balance_slide_ticks = ticks;
    pair[0].balance_slide_ticks = ticks;
}

/* B8-BD: the same controls as A0-B7 on all four effect pairs at once. */
void snd_cmd_B8(SoundCommand *cmd)
{
    SeqChannel *pair = &seq_channels[54];

    snd_effect_balance(cmd, pair);
    snd_effect_balance(cmd, pair - 2);
    snd_effect_balance(cmd, pair - 4);
    snd_effect_balance(cmd, pair - 6);
}

void snd_cmd_B9(SoundCommand *cmd)
{
    SeqChannel *pair = &seq_channels[54];

    snd_effect_balance_slide(cmd, pair);
    snd_effect_balance_slide(cmd, pair - 2);
    snd_effect_balance_slide(cmd, pair - 4);
    snd_effect_balance_slide(cmd, pair - 6);
}

void snd_cmd_A0(SoundCommand *cmd)
{
    snd_effect_balance(cmd, &seq_channels[52]);
}

void snd_cmd_A4(SoundCommand *cmd)
{
    snd_effect_balance_slide(cmd, &seq_channels[52]);
}

void snd_cmd_A1(SoundCommand *cmd)
{
    snd_effect_balance(cmd, &seq_channels[50]);
}

void snd_cmd_A5(SoundCommand *cmd)
{
    snd_effect_balance_slide(cmd, &seq_channels[50]);
}

void snd_cmd_A2(SoundCommand *cmd)
{
    snd_effect_balance(cmd, &seq_channels[48]);
}

void snd_cmd_A6(SoundCommand *cmd)
{
    snd_effect_balance_slide(cmd, &seq_channels[48]);
}

void snd_cmd_A3(SoundCommand *cmd)
{
    snd_effect_balance(cmd, &seq_channels[54]);
}

void snd_cmd_A7(SoundCommand *cmd)
{
    snd_effect_balance_slide(cmd, &seq_channels[54]);
}

void snd_effect_pan(SoundCommand *cmd, SeqChannel *pair)
{
    unsigned short value = (cmd->arg[0] & 0x7F) << 8;

    pair[1].pan_slide_ticks = 0;
    pair[0].pan_slide_ticks = 0;
    pair[1].pan = value;
    pair[0].pan = value;
    pair[0].voice_update |= UPDATE_VOLUME;
    pair[1].voice_update |= UPDATE_VOLUME;
}

void snd_effect_pan_slide(SoundCommand *cmd, SeqChannel *pair)
{
    unsigned short ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    pair[0].pan_slide_step = (short)(((cmd->arg[1] & 0x7F) << 8) - pair[0].pan) / (short)ticks;
    pair[1].pan_slide_step = (short)(((cmd->arg[1] & 0x7F) << 8) - pair[1].pan) / (short)ticks;
    pair[1].pan_slide_ticks = ticks;
    pair[0].pan_slide_ticks = ticks;
}

void snd_cmd_BA(SoundCommand *cmd)
{
    SeqChannel *pair = &seq_channels[54];

    snd_effect_pan(cmd, pair);
    snd_effect_pan(cmd, pair - 2);
    snd_effect_pan(cmd, pair - 4);
    snd_effect_pan(cmd, pair - 6);
}

void snd_cmd_BB(SoundCommand *cmd)
{
    SeqChannel *pair = &seq_channels[54];

    snd_effect_pan_slide(cmd, pair);
    snd_effect_pan_slide(cmd, pair - 2);
    snd_effect_pan_slide(cmd, pair - 4);
    snd_effect_pan_slide(cmd, pair - 6);
}

void snd_cmd_A8(SoundCommand *cmd)
{
    snd_effect_pan(cmd, &seq_channels[52]);
}

void snd_cmd_AC(SoundCommand *cmd)
{
    snd_effect_pan_slide(cmd, &seq_channels[52]);
}

void snd_cmd_A9(SoundCommand *cmd)
{
    snd_effect_pan(cmd, &seq_channels[50]);
}

void snd_cmd_AD(SoundCommand *cmd)
{
    snd_effect_pan_slide(cmd, &seq_channels[50]);
}

void snd_cmd_AA(SoundCommand *cmd)
{
    snd_effect_pan(cmd, &seq_channels[48]);
}

void snd_cmd_AE(SoundCommand *cmd)
{
    snd_effect_pan_slide(cmd, &seq_channels[48]);
}

void snd_cmd_AB(SoundCommand *cmd)
{
    snd_effect_pan(cmd, &seq_channels[54]);
}

void snd_cmd_AF(SoundCommand *cmd)
{
    snd_effect_pan_slide(cmd, &seq_channels[54]);
}

void snd_effect_pitch_shift(SoundCommand *cmd, SeqChannel *pair)
{
    int value = (signed char)cmd->arg[0] << 8;

    pair[1].pitch_shift_slide_ticks = 0;
    pair[0].pitch_shift_slide_ticks = 0;
    pair[1].pitch_shift = value;
    pair[0].pitch_shift = value;
    pair[0].voice_update |= UPDATE_PITCH;
    pair[1].voice_update |= UPDATE_PITCH;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_effect_pitch_shift_slide);

void snd_cmd_BC(SoundCommand *cmd)
{
    SeqChannel *pair = &seq_channels[54];

    snd_effect_pitch_shift(cmd, pair);
    snd_effect_pitch_shift(cmd, pair - 2);
    snd_effect_pitch_shift(cmd, pair - 4);
    snd_effect_pitch_shift(cmd, pair - 6);
}

void snd_cmd_BD(SoundCommand *cmd)
{
    SeqChannel *pair = &seq_channels[54];

    snd_effect_pitch_shift_slide(cmd, pair);
    snd_effect_pitch_shift_slide(cmd, pair - 2);
    snd_effect_pitch_shift_slide(cmd, pair - 4);
    snd_effect_pitch_shift_slide(cmd, pair - 6);
}

void snd_cmd_B0(SoundCommand *cmd)
{
    snd_effect_pitch_shift(cmd, &seq_channels[52]);
}

void snd_cmd_B4(SoundCommand *cmd)
{
    snd_effect_pitch_shift_slide(cmd, &seq_channels[52]);
}

void snd_cmd_B1(SoundCommand *cmd)
{
    snd_effect_pitch_shift(cmd, &seq_channels[50]);
}

void snd_cmd_B5(SoundCommand *cmd)
{
    snd_effect_pitch_shift_slide(cmd, &seq_channels[50]);
}

void snd_cmd_B2(SoundCommand *cmd)
{
    snd_effect_pitch_shift(cmd, &seq_channels[48]);
}

void snd_cmd_B6(SoundCommand *cmd)
{
    snd_effect_pitch_shift_slide(cmd, &seq_channels[48]);
}

void snd_cmd_B3(SoundCommand *cmd)
{
    snd_effect_pitch_shift(cmd, &seq_channels[54]);
}

void snd_cmd_B7(SoundCommand *cmd)
{
    snd_effect_pitch_shift_slide(cmd, &seq_channels[54]);
}

/* D0 v: a value at 0x80062FE8 = v (signed) << 16. D1 t v slides it over t ticks (0 = 1). D4/D5 do the same for
 * 0x80062FE4. What they control isn't known yet. */
void snd_cmd_D0(SoundCommand *cmd)
{
    D_80062FE8 = (signed char)cmd->arg[0] << 16;
    D_80062F48 = 0;
}

void snd_cmd_D1(SoundCommand *cmd)
{
    int ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    D_80062F30 = (((signed char)cmd->arg[1] << 16) - D_80062FE8) / ticks;
    D_80062F48 = ticks;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_cmd_D2);

void snd_cmd_D4(SoundCommand *cmd)
{
    D_80062FE4 = (signed char)cmd->arg[0] << 16;
    D_80062F40 = 0;
}

void snd_cmd_D5(SoundCommand *cmd)
{
    int ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    D_80062F28 = (((signed char)cmd->arg[1] << 16) - D_80062FE4) / ticks;
    D_80062F40 = ticks;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_cmd_D6);

void snd_cmd_F0(SoundCommand *cmd)
{
    func_80029F44();
}

void snd_cmd_F1(SoundCommand *cmd)
{
    func_8002A43C();
}

void snd_cmd_80(SoundCommand *cmd)
{
    seq_song.unk_00 = 1;
    func_8002A748();
    func_8002A798();
}

void snd_cmd_82(SoundCommand *cmd)
{
    seq_song.unk_00 = 4;
    func_8002A748();
    func_8002A798();
}

void snd_cmd_81(SoundCommand *cmd)
{
    seq_song.unk_00 = 2;
    func_8002A748();
    func_8002A798();
}

/* 90 v: stores v, then every music channel rewrites its volume. */
void snd_cmd_90(SoundCommand *cmd)
{
    unsigned short i;
    SeqChannel *ch;

    D_80062FD8 = cmd->arg[0];
    for (i = 0, ch = seq_channels; i < 24; i++, ch++)
        ch->voice_update |= UPDATE_VOLUME;
}

/* 92 n: the song's branch level, which EF tests. */
void snd_cmd_92(SoundCommand *cmd)
{
    seq_song_branch_level = cmd->arg[0];
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002C8DC);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002C9E4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CA84);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CB78);

void snd_cmd_E0(SoundCommand *cmd)
{
    D_80062F70 = cmd->arg[0] & 0x7F;
    seq_song_flags |= SONG_UPDATE_REVERB;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CC44);

void snd_cmd_F2(SoundCommand *cmd)
{
    D_8008337E = 0;
}

void snd_cmd_F3(SoundCommand *cmd)
{
    D_800833DE = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CCDC);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CDD0);

/* F8/F9: after func_8002CFC0, the voices at 0x80062F00 leave the effects, and their reverb goes off (F8) or on (F9).
 * The game reaches effect_voices through a pointer here. */
void snd_cmd_F8(SoundCommand *cmd)
{
    unsigned int *voices = &effect_voices;

    func_8002CFC0(cmd);
    *voices &= ~D_80062F00;
    effect_reverb_voices &= ~D_80062F00;
    func_80030038();
}

void snd_cmd_F9(SoundCommand *cmd)
{
    unsigned int *voices = &effect_voices;

    func_8002CFC0(cmd);
    *voices &= ~D_80062F00;
    effect_reverb_voices |= D_80062F00;
    func_80030038();
}

void snd_cmd_FA(SoundCommand *cmd)
{
    func_80029A50();
}

void func_8002CF98(void) {
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CFA0);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CFC0);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D1E4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D2D4);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D410);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D4A0);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D530);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D668);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D7A0);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002D8E8);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002DA30);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002DA7C);

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_run_command);

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_flush_queue);

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_update_voice);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002E428);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_channel_slides);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002E954);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_channel_lfo);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002F24C);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_layer_voice_update);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002F848);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002FDA0);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002FE48);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002FF4C);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80030038);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80030148);

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_timer_tick);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80030380);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_tick);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_step_channel);

/* A program change: the instrument's sample, loop and whole envelope. */
void load_channel_instrument(SeqChannel *ch, unsigned short instrument)
{
    Instrument *inst;

    ch->instrument = instrument;
    inst = &instrument_table[instrument];
    ch->sample = inst->sample;
    ch->loop = inst->loop;
    ch->attack_mode = inst->adsr[5];
    ch->sustain_mode = inst->adsr[6];
    ch->release_mode = inst->adsr[7];
    ch->attack_rate = inst->adsr[0];
    ch->decay_rate = inst->adsr[1];
    ch->sustain_level = inst->adsr[2];
    ch->sustain_rate = inst->adsr[3];
    ch->release_rate = inst->adsr[4];
    ch->voice_update |= UPDATE_SAMPLE | UPDATE_ENVELOPE | UPDATE_LOOP;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_look_ahead);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80031A70);

void seq_cmd_tempo(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->tempo = *ch->pos++ << 16;
    song->tempo |= *ch->pos++ << 24;
    song->tempo_slide_ticks = 0;
}

/* E9 t lo hi: slide the tempo over t ticks (0 = 256). */
void seq_cmd_tempo_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;
    int target;
    int value;

    song->tempo_slide_ticks = ticks;
    if (!ticks)
        song->tempo_slide_ticks = 256;
    target = *ch->pos++ << 16;
    target |= *ch->pos++ << 24;
    value = song->tempo & 0xFFFF0000;
    song->tempo_slide_step = (target - value) / song->tempo_slide_ticks;
    song->tempo = value;
}

void seq_cmd_reverb_depth(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int depth = *ch->pos++ << 16;

    depth |= *ch->pos++ << 24;
    song->reverb_slide_ticks = 0;
    song->flags |= SONG_UPDATE_REVERB;
    song->reverb_depth = depth;
}

/* EB t lo hi: slide the reverb depth over t ticks (0 = 256). */
void seq_cmd_reverb_depth_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;
    int target;
    int value;

    song->reverb_slide_ticks = ticks;
    if (!ticks)
        song->reverb_slide_ticks = 256;
    target = *ch->pos++ << 16;
    target |= *ch->pos++ << 24;
    value = song->reverb_depth & 0xFFFF0000;
    song->reverb_slide_step = (target - value) / song->reverb_slide_ticks;
    song->reverb_depth = value;
}

void seq_cmd_volume_scale(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->volume_scale = *ch->pos++;
    ch->voice_update |= UPDATE_VOLUME;
}

void seq_cmd_volume(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->volume = *(signed char *)ch->pos++ << 23;
    ch->volume_slide_ticks = 0;
    ch->voice_update |= UPDATE_VOLUME;
}

/* A9 t v: slide the volume to v over t ticks (0 = 256), from the current volume's top 16 bits. */
void seq_cmd_volume_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;
    int target;
    int value;

    ch->volume_slide_ticks = ticks;
    if (!ticks)
        ch->volume_slide_ticks = 256;
    target = *(signed char *)ch->pos++ << 23;
    value = ch->volume & 0xFFFF0000;
    ch->volume_slide_step = (target - value) / ch->volume_slide_ticks;
    ch->volume = value;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_cmd_layer_on);

void seq_cmd_layer_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int layer = (unsigned short)ch->layer_voice;

    if (D_80062F04)
        layer -= 24;
    if (ch->flags & CHANNEL_LAYER) {
        ch->flags &= ~CHANNEL_LAYER;
        song->layer_voices &= ~(1 << layer);
    }
}

void seq_cmd_layer_balance(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->balance = *ch->pos++ << 8;
    ch->balance_slide_ticks = 0;
    if (ch->flags & CHANNEL_LAYER)
        ch->voice_update |= UPDATE_VOLUME;
}

void seq_cmd_layer_balance_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;
    short value;

    ch->balance_slide_ticks = ticks;
    if (!ticks)
        ch->balance_slide_ticks = 256;
    value = ch->balance & 0xFF00;
    ch->balance_slide_step = ((*ch->pos++ << 8) - value) / ch->balance_slide_ticks;
    ch->balance = value;
}

void seq_cmd_pan(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->pan = *ch->pos++ << 8;
    ch->pan_slide_ticks = 0;
    ch->voice_update |= UPDATE_VOLUME;
}

void seq_cmd_pan_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;
    int value;

    ch->pan_slide_ticks = ticks;
    if (!ticks)
        ch->pan_slide_ticks = 256;
    value = ch->pan & 0xFF00;
    ch->pan_slide_step = ((*ch->pos++ << 8) - value) / ch->pan_slide_ticks;
    ch->pan = value;
}

void seq_cmd_set_octave(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->octave = *ch->pos++;
}

void seq_cmd_octave_up(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->octave = (ch->octave + 1) & 0xF;
}

/* The game adds 0xFFFF here, not -1: the step is an unsigned short. */
void seq_cmd_octave_down(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->octave = (ch->octave + (unsigned short)-1) & 0xF;
}

/* A1 n: instrument n. It frees an F4 layer's voice, rescales a sounding note's pitch to the new instrument (unless
 * an effect holds the voice), and with F8 overlap on keeps the release rate and mode. */
void seq_cmd_instrument(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int layer = (unsigned short)ch->layer_voice;
    int n = *ch->pos++;
    Instrument *inst;

    if (D_80062F04)
        layer -= 24;
    if (ch->flags & CHANNEL_LAYER) {
        song->layer_voices &= ~(1 << layer);
        ch->flags &= ~CHANNEL_LAYER;
    }
    if (ch->is_effect || !(song->update_voices & voice & effect_voices)) {
        ch->voice_update |= UPDATE_PITCH;
        ch->note_pitch = ch->note_pitch * instrument_table[n].pitch[0] / instrument_table[ch->instrument].pitch[0];
    }
    if (ch->flags & CHANNEL_OVERLAP) {
        inst = &instrument_table[n];
        ch->instrument = n;
        ch->sample = inst->sample;
        ch->loop = inst->loop;
        ch->attack_rate = inst->adsr[0];
        ch->decay_rate = inst->adsr[1];
        ch->sustain_level = inst->adsr[2];
        ch->sustain_rate = inst->adsr[3];
        ch->attack_mode = inst->adsr[5];
        ch->sustain_mode = inst->adsr[6];
        ch->voice_update |= UPDATE_SAMPLE | UPDATE_ATTACK_MODE | UPDATE_SUSTAIN_MODE | UPDATE_ATTACK_RATE |
                            UPDATE_DECAY_RATE | UPDATE_SUSTAIN_RATE | UPDATE_SUSTAIN_LEVEL | UPDATE_LOOP;
    } else {
        load_channel_instrument(ch, n);
    }
}

/* F2 n: instrument n, but notes start on the silent blocks at SPU 0x76FE0 and jump to the instrument's loop
 * point, so they play from the loop on (until the next A1). It rescales a sounding note's pitch like A1. */
void seq_cmd_instrument_from_loop(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int n = *ch->pos++;
    Instrument *inst = &instrument_table[n];

    if (ch->is_effect || !(song->update_voices & voice & effect_voices)) {
        ch->voice_update |= UPDATE_PITCH;
        ch->note_pitch = ch->note_pitch * inst->pitch[0] / instrument_table[ch->instrument].pitch[0];
    }
    ch->instrument = n;
    ch->sample = 0x76FE0;
    ch->loop = inst->loop;
    ch->attack_rate = inst->adsr[0];
    ch->decay_rate = inst->adsr[1];
    ch->sustain_level = inst->adsr[2];
    ch->sustain_rate = inst->adsr[3];
    ch->attack_mode = inst->adsr[5];
    ch->sustain_mode = inst->adsr[6];
    if (ch->flags & CHANNEL_OVERLAP) {
        ch->voice_update |= UPDATE_SAMPLE | UPDATE_ATTACK_MODE | UPDATE_SUSTAIN_MODE | UPDATE_ATTACK_RATE |
                            UPDATE_DECAY_RATE | UPDATE_SUSTAIN_RATE | UPDATE_SUSTAIN_LEVEL | UPDATE_LOOP;
    } else {
        ch->release_rate = inst->adsr[4];
        ch->release_mode = inst->adsr[7];
        ch->voice_update |= UPDATE_SAMPLE | UPDATE_ENVELOPE | UPDATE_LOOP;
    }
}

/* B3: the instrument's envelope back, on the layer voice too. */
void seq_cmd_envelope_reset(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    Instrument *inst = &instrument_table[ch->instrument];
    SeqChannel *layer;

    ch->attack_rate = inst->adsr[0];
    ch->decay_rate = inst->adsr[1];
    ch->sustain_level = inst->adsr[2];
    ch->sustain_rate = inst->adsr[3];
    ch->release_rate = inst->adsr[4];
    ch->attack_mode = inst->adsr[5];
    ch->sustain_mode = inst->adsr[6];
    ch->release_mode = inst->adsr[7];
    ch->voice_update |= UPDATE_ENVELOPE;
    if (ch->flags & CHANNEL_LAYER) {
        layer = &seq_channels[ch->layer_voice];
        layer->attack_rate = ch->attack_rate;
        layer->decay_rate = ch->decay_rate;
        layer->sustain_level = ch->sustain_level;
        layer->sustain_rate = ch->sustain_rate;
        layer->release_rate = ch->release_rate;
        layer->attack_mode = ch->attack_mode;
        layer->sustain_mode = ch->sustain_mode;
        layer->release_mode = ch->release_mode;
    }
}

void seq_cmd_transpose(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->transpose = (signed char)*ch->pos++;
}

void seq_cmd_transpose_add(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->transpose = (signed char)*ch->pos++ + ch->transpose;
}

/* A4 n s: the next note or tie bends s semitones over n ticks (0 = 256). */
void seq_cmd_bend(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    ch->bend_ticks = ticks;
    if (!ticks)
        ch->bend_ticks = 256;
    ch->bend_semitones = (signed char)*ch->pos++;
}

/* DA n: the next notes glide from the last one over n ticks (0 = 256). It also turns slur mode on. */
void seq_cmd_glide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    ch->glide_ticks = ticks;
    if (!ticks)
        ch->glide_ticks = 256;
    ch->last_transpose = 0;
    ch->last_key = 0;
    ch->note_mode = 1;
}

void seq_cmd_glide_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->glide_ticks = 0;
}

void seq_cmd_detune(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->detune = (signed char)*ch->pos++;
}

void seq_cmd_detune_add(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->detune = (signed char)*ch->pos++ + ch->detune;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_cmd_vibrato);

/* B5 d: vibrato depth. The amount scales with the note's pitch: in full (top bit set) or by 15/256. */
void seq_cmd_vibrato_depth(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    unsigned int depth = *ch->pos++ << 8;
    int pitch = ch->note_pitch;
    int amount;

    ch->vibrato_depth = depth;
    amount = (depth & 0x7F00) >> 8;
    ch->vibrato_amount = (amount * (depth & 0x8000 ? pitch : (pitch * 15) >> 8)) >> 7;
}

/* DD t d: slide the vibrato depth to d over t ticks (0 = 256). */
void seq_cmd_vibrato_depth_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    if (!ticks)
        ticks = 256;
    ch->vibrato_slide_step = ((*ch->pos++ << 8) - ch->vibrato_depth) / ticks;
    ch->vibrato_slide_ticks = ticks;
}

void seq_cmd_vibrato_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags &= ~CHANNEL_VIBRATO;
    ch->vibrato_offset = 0;
    ch->voice_update |= UPDATE_PITCH;
}

/* B8 d r w: tremolo on, restarted on the spot. Effects take no delay: a non-zero first byte is the depth. */
void seq_cmd_tremolo(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int rate;
    int depth;

    ch->flags |= CHANNEL_TREMOLO;
    if (ch->is_effect) {
        ch->tremolo_delay = 0;
        depth = *ch->pos++;
        if (depth)
            ch->tremolo_depth = depth << 8;
    } else {
        ch->tremolo_delay = *ch->pos++;
    }
    rate = *ch->pos++;
    ch->tremolo_rate = rate;
    if (!rate)
        ch->tremolo_rate = 256;
    ch->tremolo_wave = *ch->pos++;
    ch->tremolo_wave_pos = lfo_waveforms[ch->tremolo_wave];
    ch->tremolo_delay_count = ch->tremolo_delay;
    ch->tremolo_step_count = 1;
}

void seq_cmd_tremolo_depth(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->tremolo_depth = *ch->pos++ << 8;
}

void seq_cmd_tremolo_depth_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    if (!ticks)
        ticks = 256;
    ch->tremolo_slide_step = ((*ch->pos++ << 8) - ch->tremolo_depth) / ticks;
    ch->tremolo_slide_ticks = ticks;
}

void seq_cmd_tremolo_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags &= ~CHANNEL_TREMOLO;
    ch->tremolo_offset = 0;
    ch->voice_update |= UPDATE_VOLUME;
}

/* BC r w: pan LFO on, a waveform step every r ticks (0 = 256), restarted on the spot. */
void seq_cmd_pan_lfo(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int rate;

    ch->flags |= CHANNEL_PAN_LFO;
    rate = *ch->pos++;
    ch->pan_lfo_rate = rate;
    if (!rate)
        ch->pan_lfo_rate = 256;
    ch->pan_lfo_wave = *ch->pos++;
    ch->pan_lfo_wave_pos = lfo_waveforms[ch->pan_lfo_wave];
    ch->pan_lfo_step_count = 1;
}

void seq_cmd_pan_lfo_depth(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->pan_lfo_depth = *ch->pos++ << 7;
}

void seq_cmd_pan_lfo_depth_slide(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    if (!ticks)
        ticks = 256;
    ch->pan_lfo_slide_step = ((*ch->pos++ << 7) - ch->pan_lfo_depth) / ticks;
    ch->pan_lfo_slide_ticks = ticks;
}

void seq_cmd_pan_lfo_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags &= ~CHANNEL_PAN_LFO;
    ch->pan_lfo_offset = 0;
    ch->voice_update |= UPDATE_VOLUME;
}

void seq_cmd_noise_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->noise_voices |= voice;
    else
        effect_noise_voices |= voice;
    seq_song_flags |= SONG_UPDATE_NOISE;
    func_8002FF4C();
}

void seq_cmd_noise_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->noise_voices &= ~voice;
    else
        effect_noise_voices &= ~voice;
    seq_song_flags |= SONG_UPDATE_NOISE;
    func_8002FF4C();
    ch->noise_ticks = 0;
}

/* An effect's first channel sits on an even voice (0x555555), which never takes FM. */
void seq_cmd_fm_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->fm_voices |= voice;
    else if (!(voice & 0x555555))
        effect_fm_voices |= voice;
    func_80030148();
}

void seq_cmd_fm_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->fm_voices &= ~voice;
    else
        effect_fm_voices &= ~voice;
    func_80030148();
    ch->fm_ticks = 0;
}

void seq_cmd_reverb_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->reverb_voices |= voice;
    else
        effect_reverb_voices |= voice;
    func_80030038();
}

void seq_cmd_reverb_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->reverb_voices &= ~voice;
    else
        effect_reverb_voices &= ~voice;
    func_80030038();
}

void seq_cmd_slur_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->note_mode = 1;
}

void seq_cmd_slur_off(SeqChannel *ch, SeqSong *song, unsigned int voice) {
}

void seq_cmd_full_length_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->note_mode = 4;
}

void seq_cmd_full_length_off(SeqChannel *ch, SeqSong *song, unsigned int voice) {
}

/* AC n: the noise clock (0-63). With either top bit set, n & 0x3F is added instead, wrapping at 64. */
void seq_cmd_noise_clock(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    unsigned int clock = *ch->pos++;

    if (!ch->is_effect) {
        if (clock & 0xC0)
            song->noise_clock = (song->noise_clock + (clock & 0x3F)) & 0x3F;
        else
            song->noise_clock = clock;
    } else {
        if (clock & 0xC0)
            effect_noise_clock = (effect_noise_clock + (clock & 0x3F)) & 0x3F;
        else
            effect_noise_clock = clock;
    }
    seq_song_flags |= SONG_UPDATE_NOISE;
}

void seq_cmd_attack_rate(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->attack_rate = *ch->pos++;
    ch->voice_update |= UPDATE_ATTACK_RATE | UPDATE_ATTACK_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].attack_rate = ch->attack_rate;
}

void seq_cmd_decay_rate(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->decay_rate = *ch->pos++;
    ch->voice_update |= UPDATE_DECAY_RATE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].decay_rate = ch->decay_rate;
}

void seq_cmd_sustain_level(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->sustain_level = *ch->pos++;
    ch->voice_update |= UPDATE_SUSTAIN_LEVEL;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].sustain_level = ch->sustain_level;
}

void seq_cmd_sustain_rate(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->sustain_rate = *ch->pos++;
    ch->voice_update |= UPDATE_SUSTAIN_RATE | UPDATE_SUSTAIN_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].sustain_rate = ch->sustain_rate;
}

void seq_cmd_release_rate(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->release_rate = *ch->pos++;
    ch->voice_update |= UPDATE_RELEASE_RATE | UPDATE_RELEASE_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].release_rate = ch->release_rate;
}

void seq_cmd_attack_mode(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->attack_mode = *ch->pos++;
    ch->voice_update |= UPDATE_ATTACK_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].attack_mode = ch->attack_mode;
}

void seq_cmd_sustain_mode(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->sustain_mode = *ch->pos++;
    ch->voice_update |= UPDATE_SUSTAIN_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].sustain_mode = ch->sustain_mode;
}

void seq_cmd_release_mode(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->release_mode = *ch->pos++;
    ch->voice_update |= UPDATE_RELEASE_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].release_mode = ch->release_mode;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_cmd_overlap_on);

/* F9: frees the second voice, and the release rate goes back to the instrument's. */
void seq_cmd_overlap_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->overlap_voices &= ~(1 << ch->overlap_voice);
    ch->flags &= ~CHANNEL_OVERLAP;
    ch->release_rate = instrument_table[ch->instrument].adsr[4];
    ch->voice_update |= UPDATE_RELEASE_RATE | UPDATE_RELEASE_MODE;
}

void seq_cmd_loop_point(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->loop_level = (ch->loop_level + 1) & 3;
    ch->loop_start[ch->loop_level] = ch->pos;
    ch->loop_count[ch->loop_level] = 0;
}

/* C9 n: back to the loop point until the pass count reaches n (0 = 256), then pop the loop level. */
void seq_cmd_repeat(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int count = *ch->pos++;

    if (!count)
        count = 256;
    if (++ch->loop_count[ch->loop_level] != count)
        ch->pos = ch->loop_start[ch->loop_level];
    else
        ch->loop_level = (ch->loop_level + (unsigned short)-1) & 3;
}

/* F0 n lo hi: on pass n of the current repeat (0 = 256), jump by a signed offset from after the arguments. */
void seq_cmd_repeat_branch(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int count = *ch->pos++;
    int offset;

    if (!count)
        count = 256;
    if (ch->loop_count[ch->loop_level] + 1 != count) {
        ch->pos += 2;
        return;
    }
    offset = *ch->pos++;
    offset |= *ch->pos++ << 8;
    ch->pos += (short)offset;
}

/* F1 n lo hi: F0 that also leaves the loop. */
void seq_cmd_repeat_break(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int count = *ch->pos++;
    int offset;

    if (!count)
        count = 256;
    if (ch->loop_count[ch->loop_level] + 1 != count) {
        ch->pos += 2;
        return;
    }
    offset = *ch->pos++;
    offset |= *ch->pos++ << 8;
    ch->pos += (short)offset;
    ch->loop_level = (ch->loop_level + (unsigned short)-1) & 3;
}

void seq_cmd_loop_forever(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->loop_count[ch->loop_level]++;
    ch->pos = ch->loop_start[ch->loop_level];
}

/* A2: the next note's length, once. */
void seq_cmd_next_length(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    ch->fixed_length = 0;
    ch->gate = ticks | ticks << 8;
    ch->last_length = ticks;
}

/* DC n: every note lasts the last length + n ticks (signed), clamped to 1-255; 0 turns it off. */
void seq_cmd_fixed_length(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    short ticks = (signed char)*ch->pos++;

    if (ticks) {
        ticks += ch->last_length;
        if (ticks <= 0)
            ticks = 1;
        else if (ticks >= 256)
            ticks = 255;
    }
    ch->fixed_length = ticks;
}

/* EC lo hi: drum-kit mode, with the key map at a signed offset from after the arguments. */
void seq_cmd_drum_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int offset = *ch->pos++;

    offset |= *ch->pos++ << 8;
    ch->drum_map = ch->pos + (short)offset;
    ch->flags |= CHANNEL_DRUMS;
}

void seq_cmd_drum_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags &= ~CHANNEL_DRUMS;
}

void seq_cmd_time_signature(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->beat_ticks = *ch->pos++;
    song->beats_per_bar = *ch->pos++;
    song->bar_beat = 0;
    song->beat_tick = 0;
}

void seq_cmd_bar_number(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->bar_number = *ch->pos++;
    song->bar_number |= *ch->pos++ << 8;
}

void func_800335CC(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->unk_54 = 1;
}

void seq_cmd_decay_sustain(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    seq_cmd_decay_rate(ch, song, voice);
    seq_cmd_sustain_level(ch, song, voice);
}

/* CE n: noise on now, and flipped after n + 1 ticks (0 = 257). */
void seq_cmd_noise_on_timed(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    ch->noise_ticks = ticks ? ticks + 1 : 257;
    seq_cmd_noise_on(ch, song, voice);
}

/* CF n: noise flipped after n + 1 ticks (0 = 257). */
void seq_cmd_noise_timed(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    ch->noise_ticks = ticks ? ticks + 1 : 257;
}

/* D2 n: FM on now, and flipped after n + 1 ticks (0 = 257). */
void seq_cmd_fm_on_timed(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    ch->fm_ticks = ticks ? ticks + 1 : 257;
    seq_cmd_fm_on(ch, song, voice);
}

/* D3 n: FM flipped after n + 1 ticks (0 = 257). */
void seq_cmd_fm_timed(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int ticks = *ch->pos++;

    ch->fm_ticks = ticks ? ticks + 1 : 257;
}

/* CB: LFOs, flags 0x10/0x20, noise, FM, reverb, slur and full length all off. */
void seq_cmd_all_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags &= ~(CHANNEL_VIBRATO | CHANNEL_TREMOLO | CHANNEL_PAN_LFO | CHANNEL_PITCH_FOLLOW | CHANNEL_VOLUME_FOLLOW);
    seq_cmd_noise_off(ch, song, voice);
    seq_cmd_fm_off(ch, song, voice);
    seq_cmd_reverb_off(ch, song, voice);
    ch->note_mode &= ~5;
}

void seq_cmd_pitch_follow_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags |= CHANNEL_PITCH_FOLLOW;
}

void seq_cmd_pitch_follow_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags &= ~CHANNEL_PITCH_FOLLOW;
}

void seq_cmd_volume_follow_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags |= CHANNEL_VOLUME_FOLLOW;
}

void seq_cmd_volume_follow_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->flags &= ~CHANNEL_VOLUME_FOLLOW;
}

void seq_cmd_jump(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int offset = *ch->pos++;

    offset |= *ch->pos++ << 8;
    ch->pos += (short)offset;
}

/* EF n lo hi: jump when the game's branch level is set and at least n. */
void seq_cmd_branch_if(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int n = *ch->pos++;
    int offset;

    if (song->branch_level && song->branch_level >= n) {
        offset = *ch->pos++;
        offset |= *ch->pos++ << 8;
        ch->pos += (short)offset;
        song->branch_taken = n;
    } else {
        ch->pos += 2;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_cmd_end);

/* Every unused command byte ends the channel, as A0 does. */
void seq_cmd_unknown(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    seq_cmd_end(ch, song, voice);
}
