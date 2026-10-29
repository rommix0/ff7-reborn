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

/* Stops the stream (sound command FA, or the fill at its end): no transfer callback or IRQ, its voices off, and
 * channels 16 and 17 rewrite everything when they next play. */
void snd_stream_stop(void)
{
    unsigned int voices;

    func_80038FEC(0);
    func_80038A84(0);
    SpuSetIRQCallback(0);
    func_80038C6C(0, snd_stream_voices);
    voices = snd_stream_voices;
    if (voices & 0x10000)
        seq_channels[16].voice.update = UPDATE_ALL;
    if (voices & 0x20000)
        seq_channels[17].voice.update = UPDATE_ALL;
    snd_stream_voices = 0;
    snd_apply_reverb_voices();
    snd_apply_fm_voices();
    snd_apply_noise_voices();
}

/* A song's reverb mode. A change turns reverb off, sets the mode with its work area cleared (0x100), and turns it
 * back on. */
void snd_set_reverb_mode(unsigned int mode)
{
    SpuReverbAttr *attr = &snd_reverb_attr;

    snd_stream_stop();
    SpuGetReverbModeParam(attr);
    if (snd_reverb_attr.mode == mode)
        return;
    seq_song.reverb_mode = mode;
    SpuSetReverb(0);
    snd_reverb_attr.mode = mode | 0x100;
    attr->mask = 1;
    SpuSetReverbModeParam(attr);
    SpuSetReverb(1);
}

void snd_load_song(unsigned int *data, unsigned int size)
{
    unsigned int *dst;

    size >>= 2;
    dst = song_data;
    while (size) {
        size--;
        *dst++ = *data++;
    }
}

/* A channel's defaults at the start of a song or effect. */
void channel_reset(SeqChannel *ch, unsigned char *start)
{
    ch->pos = start;
    ch->volume_scale = 120;
    load_channel_instrument(ch, 5);
    ch->octave = 2;
    ch->detune = 0;
    ch->transpose = 0;
    ch->glide_ticks = 0;
    ch->unk_34 = 0;
    ch->bend_semitones = 0;
    ch->fixed_length = 0;
    ch->last_length = 0;
    ch->unk_64 = 0;
    ch->volume = 100 << 23;
    ch->volume_slide_ticks = 0;
    ch->flags = 0;
    ch->loop_level = 0;
    ch->note_mode = 0;
    ch->pan_lfo_offset = 0;
    ch->pan_lfo_depth = 0;
    ch->tremolo_depth = 0;
    ch->vibrato_depth = 0;
    ch->pan_lfo_slide_ticks = 0;
    ch->tremolo_slide_ticks = 0;
    ch->vibrato_slide_ticks = 0;
    ch->fm_ticks = 0;
    ch->noise_ticks = 0;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_start_song);

/* Ends the song: every voice it holds (layers and overlaps too) keys off in 2 ticks and steps into a stub that ends
 * the channel. */
void snd_stop_song(void)
{
    unsigned int voices = seq_song_active_voices;
    SeqChannel *ch;
    unsigned int bit;

    if (!voices)
        return;
    ch = seq_channels;
    bit = 1;
    voices |= seq_song_layer_voices | seq_song_overlap_voices;
    seq_song_overlap_voices = 0;
    seq_song_layer_voices = 0;
    seq_song_update_voices = 0;
    seq_song_voices_08 = 0;
    seq_song_active_voices = voices;
    seq_song_key_off_voices |= voices;
    do {
        if (voices & bit) {
            voices ^= bit;
            ch->gate = 0x204;
            ch->pos = D_80049C40;
        }
        bit <<= 1;
        ch++;
    } while (voices);
}

/* F0: stops both songs, as snd_stop_song does the first. */
void snd_stop_songs(void)
{
    unsigned int voices = seq_song_active_voices;
    unsigned int *active;
    SeqChannel *ch;
    unsigned int bit;

    if (voices) {
        ch = seq_channels;
        bit = 1;
        voices |= seq_song_layer_voices | seq_song_overlap_voices;
        seq_song_overlap_voices = 0;
        seq_song_layer_voices = 0;
        seq_song_update_voices = 0;
        seq_song_voices_08 = 0;
        seq_song_active_voices = voices;
        seq_song_key_off_voices |= voices;
        do {
            if (voices & bit) {
                voices ^= bit;
                ch->gate = 0x204;
                ch->pos = D_80049C40;
            }
            bit <<= 1;
            ch++;
        } while (voices);
    }
    active = &seq_song2.active_voices;
    voices = *active;
    if (!voices)
        return;
    ch = &seq_channels[24];
    bit = 1;
    voices |= seq_song2.layer_voices | seq_song2.overlap_voices;
    seq_song2.overlap_voices = 0;
    seq_song2.layer_voices = 0;
    seq_song2.update_voices = 0;
    seq_song2.voices_08 = 0;
    *active = voices;
    seq_song2.key_off_voices |= voices;
    do {
        if (voices & bit) {
            voices ^= bit;
            ch->gate = 0x204;
            ch->pos = D_80049C40;
        }
        bit <<= 1;
        ch++;
    } while (voices);
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A094);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002A28C);

/* F1: every effect ends (it steps into snd_stop_song's end stub), all but a kind 2 effect on channels 54/55, whose
 * voices 22/23 stay. */
void snd_stop_effects(void)
{
    SeqChannel *ch = &seq_channels[48];
    unsigned short i;

    for (i = 48; i < 56; i++, ch++) {
        if (ch->is_effect != 2) {
            ch->gate = 0x204;
            ch->pos = D_80049C40;
        }
    }
    if (seq_channels[54].is_effect == 2) {
        effect_voices_08 &= 0xC00000;
        effect_update_voices &= 0xC00000;
        effect_key_off_voices &= ~0xC00000 & effect_voices;
    } else {
        effect_voices_08 = 0;
        effect_update_voices = 0;
        effect_key_off_voices = effect_voices;
    }
}

/* Stops pairs effect pairs, counting down from pair (channel 48 + pair + 1): their voices leave the effect masks and
 * key off, and the channels step into the end stub. */
void snd_stop_effect_pairs(unsigned short pair, int pairs)
{
    SeqChannel *ch = &seq_channels[49 + pair];
    unsigned short n = pairs * 2;
    unsigned int mask;

    switch ((unsigned short)pairs) {
    case 1:
        mask = 3 << (pair + 16);
        effect_voices_08 &= ~mask;
        effect_update_voices &= ~mask;
        effect_key_off_voices |= mask;
        break;
    case 2:
        effect_voices_08 &= ~0x3C0000;
        effect_update_voices &= ~0x3C0000;
        effect_key_off_voices |= 0x3C0000;
        break;
    case 3:
        effect_voices_08 &= ~0x3F0000;
        effect_update_voices &= ~0x3F0000;
        effect_key_off_voices |= 0x3F0000;
        break;
    case 4:
        effect_voices_08 &= ~0xFF0000;
        effect_update_voices &= ~0xFF0000;
        effect_key_off_voices |= 0xFF0000;
        break;
    }
    for (; n; n--, ch--) {
        ch->gate = 0x204;
        ch->pos = D_80049C40;
    }
}

/* An effect's two streams (0 when it has none): EFFECT.ALL's offsets 2 * id and 2 * id + 1. */
void snd_effect_streams(unsigned char **first, unsigned char **second, int effect)
{
    unsigned short i = (effect & 0x3FF) * 2;

    *first = effect_offsets[i] != 0xFFFF ? effect_data + effect_offsets[i] : 0;
    i++;
    *second = effect_offsets[i] != 0xFFFF ? effect_data + effect_offsets[i] : 0;
}

/* Every voice the song's channels hold rewrites its volume. */
void snd_refresh_song_volumes(void)
{
    unsigned int voices = seq_song.active_voices;
    SeqChannel *ch = seq_channels;
    unsigned int bit;

    if (!voices)
        return;
    bit = 1;
    do {
        if (voices & bit) {
            voices ^= bit;
            ch->voice.update |= UPDATE_VOLUME;
        }
        ch++;
        bit <<= 1;
    } while (voices);
}

/* Every voice an effect holds rewrites its volume. Effect slots 48-55 are SPU voices 16-23. */
void snd_refresh_effect_volumes(void)
{
    unsigned int voices = effect_voices;
    SeqChannel *ch = &seq_channels[48];
    unsigned int bit;

    if (!voices)
        return;
    bit = 0x10000;
    do {
        if (voices & bit) {
            voices ^= bit;
            ch->voice.update |= UPDATE_VOLUME;
        }
        ch++;
        bit <<= 1;
    } while (voices);
}

/* Before a song is saved: its voices still keyed on (or whose F8 overlap voice is, when that voice is free) stay
 * held, the rest leave its masks. A held overlap channel gets flag 0x400. */
void snd_keep_sounding_voices(void)
{
    unsigned int voices = (seq_song_active_voices | seq_song_layer_voices) & ~(effect_voices | snd_stream_voices);
    SeqChannel *ch;
    unsigned int bit;
    unsigned int spare;

    if (!voices)
        return;
    bit = 1;
    seq_song_voices_08 = seq_song_update_voices;
    ch = seq_channels;
    do {
        if (voices & bit) {
            if (SpuGetKeyStatus(bit) == 1) {
                if (ch->flags & CHANNEL_OVERLAP)
                    ch->flags |= 0x400;
                seq_song_voices_08 |= bit;
            } else if ((ch->flags & 0x600) == 0x600
                       && (~(effect_voices | snd_stream_voices) & (spare = 1 << ch->overlap_voice))
                       && SpuGetKeyStatus(spare) == 1) {
                seq_song_voices_08 |= bit;
            } else {
                seq_song_voices_08 &= ~bit;
                seq_song_update_voices &= ~bit;
            }
            voices ^= bit;
        }
        ch++;
        bit <<= 1;
    } while (voices);
}

/* Before F4 sets the effects aside: an effect that loops forever (its stream ends in CA) stays, held while its voice
 * is still keyed on, and rewrites everything when it comes back; one that ends is dropped (unless it's kind 2). */
void snd_keep_looping_effects(void)
{
    unsigned int *state = &effect_voices;
    unsigned int voices = *state;
    unsigned int *held;
    unsigned int bit = 0x10000;
    SeqChannel *ch;

    if (!voices)
        return;
    ch = &seq_channels[48];
    held = state;
    effect_voices_08 = effect_update_voices;
    do {
        if (voices & bit) {
            if (func_80031A70(ch) == 0xCA) {
                if (SpuGetKeyStatus(bit) == 1) {
                    effect_voices_08 |= bit;
                } else {
                    effect_voices_08 &= ~bit;
                    effect_update_voices &= ~bit;
                }
                ch->voice.update = UPDATE_ALL;
            } else if (ch->is_effect != 2) {
                *held &= ~bit;
                effect_voices_08 &= ~bit;
                effect_update_voices &= ~bit;
            }
            voices ^= bit;
        }
        ch++;
        bit <<= 1;
    } while (voices);
}

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_resume_song);

/* 18/19, while a song plays: it moves to the second block (channels, state and its data 12 KB up, every pointer
 * moved with it) to fade out there, channel n over (24 - n) * snd_fade_ticks ticks, while the new song starts. */
void snd_fade_out_song(void)
{
    unsigned int *src;
    unsigned int *dst;
    unsigned int *song_src;
    unsigned int *song_dst;
    unsigned int *data_src;
    unsigned int *data_dst;
    SeqChannel *ch;
    unsigned short i;
    unsigned short left;

    i = sizeof(SeqChannel) * 24 / 4;
    src = (unsigned int *)seq_channels;
    dst = (unsigned int *)&seq_channels[24];
    do {
        i--;
        *dst++ = *src++;
    } while (i);
    i = sizeof(SeqSong) / 4;
    song_src = (unsigned int *)&seq_song;
    song_dst = song_src + sizeof(SeqSong) / 4;
    do {
        i--;
        *song_dst++ = *song_src++;
    } while (i);
    data_src = song_data;
    data_dst = song_data + 0xC00;
    i = 0xC00;
    do {
        i--;
        *data_dst++ = *data_src++;
    } while (i);
    ch = &seq_channels[24];
    i = 24;
    do {
        i--;
        ch->pos += 0x3000;
        ch->drum_map += 0x3000;
        ch->loop_start[0] += 0x3000;
        ch->loop_start[1] += 0x3000;
        ch->loop_start[2] += 0x3000;
        ch->loop_start[3] += 0x3000;
        ch->layer_voice += 24;
        ch++;
    } while (i);
    for (i = 0, left = 24; i < 24; i++) {
        snd_channel_fades[i].volume = 0x7F8000;
        snd_channel_fades[i].step = -(0x7F8000 / (snd_fade_ticks * left));
        snd_channel_fades[i].ticks = left * snd_fade_ticks;
        left--;
    }
    D_80062F68 = 0xFFFFFF;
    D_80062FF8 &= ~0x100;
}

/* Copies the playing song's 24 channels and its state into a save slot. */
void snd_save_song(SeqChannel *channels, SeqChannel *saved_channels, SeqSong *song, SeqSong *saved)
{
    unsigned int *src = (unsigned int *)channels;
    unsigned int *dst = (unsigned int *)saved_channels;
    unsigned int *song_src = (unsigned int *)song;
    unsigned int *song_dst = (unsigned int *)saved;
    unsigned short n = sizeof(SeqChannel) * 24 / 4;

    do {
        n--;
        *dst++ = *src++;
    } while (n);
    n = sizeof(SeqSong) / 4;
    do {
        n--;
        *song_dst++ = *song_src++;
    } while (n);
}

/* 10 a b id: start song id. A saved song with that id is resumed instead (from either slot); song 14 is saved
 * first when it's the one playing. */
void snd_cmd_10(SoundCommand *cmd)
{
    snd_load_song(cmd->arg[0], cmd->arg[1]);
    if (seq_song_id == 14) {
        snd_keep_sounding_voices();
        snd_save_song(seq_channels, saved_song_channels[1], &seq_song, &saved_songs[1]);
    }
    snd_stop_song();
    if (saved_songs[0].song_id && saved_songs[0].song_id == (unsigned short)cmd->arg[2])
        snd_resume_song(0);
    else if (saved_songs[1].song_id && saved_songs[1].song_id == (unsigned short)cmd->arg[2])
        snd_resume_song(1);
    else
        snd_start_song();
    seq_song_id = cmd->arg[2];
}

void snd_cmd_14(SoundCommand *cmd)
{
    SeqSong *song;

    snd_load_song(cmd->arg[0], cmd->arg[1]);
    snd_keep_sounding_voices();
    song = &seq_song;
    if (seq_song_id) {
        if (seq_song_id == 14)
            snd_save_song(seq_channels, saved_song_channels[1], song, &saved_songs[1]);
        else
            snd_save_song(seq_channels, saved_song_channels[0], song, &saved_songs[0]);
    }
    snd_stop_song();
    snd_start_song();
    seq_song_id = cmd->arg[2];
}

/* 15 a b id: like 10, but a saved song with that id swaps with the playing one: the playing song goes through the
 * second block into a save slot (slot 1 if it's song 14) and the saved one resumes. */
void snd_cmd_15(SoundCommand *cmd)
{
    SeqSong *song;

    D_80062FF8 &= ~0x100;
    snd_load_song(cmd->arg[0], cmd->arg[1]);
    snd_keep_sounding_voices();
    if (saved_songs[0].song_id == (unsigned short)cmd->arg[2]) {
        snd_save_song(seq_channels, &seq_channels[24], &seq_song, &seq_song + 1);
        snd_stop_song();
        snd_resume_song(0);
        if (seq_song2.song_id == 14)
            snd_save_song(&seq_channels[24], saved_song_channels[1], &seq_song + 1, &saved_songs[1]);
        else
            snd_save_song(&seq_channels[24], saved_song_channels[0], &seq_song + 1, &saved_songs[0]);
    } else if (saved_songs[1].song_id == (unsigned short)cmd->arg[2]) {
        snd_save_song(seq_channels, &seq_channels[24], &seq_song, &seq_song + 1);
        snd_stop_song();
        snd_resume_song(1);
        if (seq_song2.song_id == 14)
            snd_save_song(&seq_channels[24], saved_song_channels[1], &seq_song + 1, &saved_songs[1]);
        else
            snd_save_song(&seq_channels[24], saved_song_channels[0], &seq_song + 1, &saved_songs[0]);
    } else {
        song = &seq_song;
        if (seq_song_id) {
            if (seq_song_id == 14)
                snd_save_song(seq_channels, saved_song_channels[1], song, &saved_songs[1]);
            else
                snd_save_song(seq_channels, saved_song_channels[0], song, &saved_songs[0]);
        }
        snd_stop_song();
        snd_start_song();
    }
    seq_song2.overlap_voices = 0;
    seq_song2.layer_voices = 0;
    seq_song2.active_voices = 0;
    seq_song_id = cmd->arg[2];
}

/* 18 / 19: while a song plays (seq_song_id set), first snd_fade_out_song with the value at 0x80062FC8 = arg 3 (0 = 16); then as 10 / 14. */
void snd_cmd_18(SoundCommand *cmd)
{
    if (seq_song_id) {
        snd_fade_ticks = cmd->arg[3] ? (unsigned short)cmd->arg[3] : 16;
        snd_fade_out_song();
    }
    snd_cmd_10(cmd);
}

void snd_cmd_19(SoundCommand *cmd)
{
    if (seq_song_id) {
        snd_fade_ticks = cmd->arg[3] ? (unsigned short)cmd->arg[3] : 16;
        snd_fade_out_song();
    }
    snd_cmd_14(cmd);
}

/* 34 first second: play two streams the caller gives, centred, on the pair at 0x34. */
void snd_cmd_34(SoundCommand *cmd)
{
    snd_stop_effect_pairs(4, 1);
    func_8002A094(0x40, 0x34, (unsigned char *)cmd->arg[0], (unsigned char *)cmd->arg[1]);
}

/* 21 pan a b: play effects a and b together, on the pairs at 0x32 and 0x34. */
void snd_cmd_21(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(4, 2);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[1]);
    func_8002A094((unsigned short)cmd->arg[0], 0x32, first, second);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[2]);
    func_8002A094((unsigned short)cmd->arg[0], 0x34, first, second);
}

/* 22 / 23 pan a b c (d): three or four effects together, on the pairs from 0x30 up. */
void snd_cmd_22(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(4, 3);
    snd_stream_stop();
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[1]);
    func_8002A094((unsigned short)cmd->arg[0], 0x30, first, second);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[2]);
    func_8002A094((unsigned short)cmd->arg[0], 0x32, first, second);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[3]);
    func_8002A094((unsigned short)cmd->arg[0], 0x34, first, second);
}

void snd_cmd_23(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(6, 4);
    snd_stream_stop();
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[1]);
    func_8002A094((unsigned short)cmd->arg[0], 0x30, first, second);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[2]);
    func_8002A094((unsigned short)cmd->arg[0], 0x32, first, second);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[3]);
    func_8002A094((unsigned short)cmd->arg[0], 0x34, first, second);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[4]);
    func_8002A094((unsigned short)cmd->arg[0], 0x36, first, second);
}

/* 30 id: play effect id through func_8002A28C. */
void snd_cmd_30(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(6, 1);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[0]);
    func_8002A28C(first, second);
}

/* 20 (and 28) / 29 / 2B pan id: play effect id on the pair at 0x34 / 0x32 / 0x36, after snd_stop_effect_pairs on it. */
void snd_cmd_20(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(4, 1);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[1]);
    func_8002A094((unsigned short)cmd->arg[0], 0x34, first, second);
}

void snd_cmd_29(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(2, 1);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[1]);
    func_8002A094((unsigned short)cmd->arg[0], 0x32, first, second);
}

/* 2A pan id: play effect id on the pair at 0x30, after snd_stop_effect_pairs on pair 0 and snd_stream_stop. */
void snd_cmd_2A(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(0, 1);
    snd_stream_stop();
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[1]);
    func_8002A094((unsigned short)cmd->arg[0], 0x30, first, second);
}

void snd_cmd_2B(SoundCommand *cmd)
{
    unsigned char *first;
    unsigned char *second;

    snd_stop_effect_pairs(6, 1);
    snd_effect_streams(&first, &second, (unsigned short)cmd->arg[1]);
    func_8002A094((unsigned short)cmd->arg[0], 0x36, first, second);
}

/* C0 v / C1 t v / C2 t a b: set or slide a value at 0x80062F5C (v & 0x7F) << 16, then call snd_refresh_song_volumes. */
void snd_cmd_C0(SoundCommand *cmd)
{
    D_80062F5C = (cmd->arg[0] & 0x7F) << 16;
    D_80062F44 = 0;
    snd_refresh_song_volumes();
}

void snd_cmd_C1(SoundCommand *cmd)
{
    int ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    D_80062F2C = (((cmd->arg[1] & 0x7F) << 16) - D_80062F5C) / ticks;
    D_80062F44 = ticks;
    snd_refresh_song_volumes();
}

void snd_cmd_C2(SoundCommand *cmd)
{
    int ticks = 1;
    int to;
    int from;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    to = (cmd->arg[2] & 0x7F) << 16;
    from = (cmd->arg[1] & 0x7F) << 16;
    D_80062F2C = (to - from) / ticks;
    D_80062F44 = ticks;
    D_80062F5C = from;
    snd_refresh_song_volumes();
}

void snd_cmd_C8(SoundCommand *cmd)
{
    snd_cd_volume = (unsigned short)cmd->arg[0] << 16;
    snd_cd_volume_slide_ticks = 0;
    snd_apply_cd_volume();
}

/* C8 v: the CD audio volume, v << 16 (applied at once). C9 t v slides it over t ticks (0 = 1); CA t a b from a to b. */
void snd_cmd_C9(SoundCommand *cmd)
{
    int ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    snd_cd_volume_slide_step = (((unsigned short)cmd->arg[1] << 16) - snd_cd_volume) / ticks;
    snd_cd_volume_slide_ticks = ticks;
}

void snd_cmd_CA(SoundCommand *cmd)
{
    int ticks = 1;
    int to;
    int from;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    to = (unsigned short)cmd->arg[2] << 16;
    from = (unsigned short)cmd->arg[1] << 16;
    snd_cd_volume_slide_step = (to - from) / ticks;
    snd_cd_volume_slide_ticks = ticks;
    snd_cd_volume = from;
}

/* Sound commands A0-B7 set or slide the balance, pan or pitch shift of a playing effect, on both channels of its
 * pair (seq_channels 48-55). A slide's length 0 means 1 tick. */
void snd_effect_balance(SoundCommand *cmd, SeqChannel *pair)
{
    short value = (cmd->arg[0] & 0x7F) << 8;

    pair[1].balance_slide_ticks = 0;
    pair[0].balance_slide_ticks = 0;
    pair[1].balance = value;
    pair[0].balance = value;
    pair[0].voice.update |= UPDATE_VOLUME;
    pair[1].voice.update |= UPDATE_VOLUME;
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
    pair[0].voice.update |= UPDATE_VOLUME;
    pair[1].voice.update |= UPDATE_VOLUME;
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
    pair[0].voice.update |= UPDATE_PITCH;
    pair[1].voice.update |= UPDATE_PITCH;
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

/* D2 a t b: D0's value from a to b over t ticks. D6: the same for D4's. */
void snd_cmd_D2(SoundCommand *cmd)
{
    int ticks = 1;
    int from = (signed char)cmd->arg[0] << 16;

    D_80062FE8 = from;
    if (cmd->arg[1])
        ticks = cmd->arg[1];
    D_80062F30 = (((signed char)cmd->arg[2] << 16) - from) / ticks;
    D_80062F48 = ticks;
}

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

void snd_cmd_D6(SoundCommand *cmd)
{
    int ticks = 1;
    int from = (signed char)cmd->arg[0] << 16;

    D_80062FE4 = from;
    if (cmd->arg[1])
        ticks = cmd->arg[1];
    D_80062F28 = (((signed char)cmd->arg[2] << 16) - from) / ticks;
    D_80062F40 = ticks;
}

void snd_cmd_F0(SoundCommand *cmd)
{
    snd_stop_songs();
}

void snd_cmd_F1(SoundCommand *cmd)
{
    snd_stop_effects();
}

void snd_cmd_80(SoundCommand *cmd)
{
    seq_song.unk_00 = 1;
    snd_refresh_song_volumes();
    snd_refresh_effect_volumes();
}

void snd_cmd_82(SoundCommand *cmd)
{
    seq_song.unk_00 = 4;
    snd_refresh_song_volumes();
    snd_refresh_effect_volumes();
}

void snd_cmd_81(SoundCommand *cmd)
{
    seq_song.unk_00 = 2;
    snd_refresh_song_volumes();
    snd_refresh_effect_volumes();
}

/* 90 v: stores v, then every music channel rewrites its volume. */
void snd_cmd_90(SoundCommand *cmd)
{
    unsigned short i;
    SeqChannel *ch;

    D_80062FD8 = cmd->arg[0];
    for (i = 0, ch = seq_channels; i < 24; i++, ch++)
        ch->voice.update |= UPDATE_VOLUME;
}

/* 92 n: the song's branch level, which EF tests. */
void snd_cmd_92(SoundCommand *cmd)
{
    seq_song_branch_level = cmd->arg[0];
}

/* 9B: pauses the music. Its voices (not the ones effects or the stream hold) go silent and hold, and the
 * song's voices move aside until 9A. */
void snd_cmd_9B(SoundCommand *cmd)
{
    unsigned int voices;
    unsigned int bit;
    unsigned int voice;

    if (seq_song_active_voices) {
        voices = (seq_song_active_voices | seq_song_layer_voices | seq_song_overlap_voices) & ~(effect_voices | snd_stream_voices);
        if (voices) {
            bit = 1;
            voice = 0;
            snd_direct_voice.volume[1] = 0;
            snd_direct_voice.volume[0] = 0;
            snd_direct_voice.sustain_rate = 0x7F;
            do {
                if (voices & bit) {
                    snd_direct_voice_update = UPDATE_VOLUME | UPDATE_SUSTAIN_MODE | UPDATE_SUSTAIN_RATE;
                    snd_update_voice(voice, &snd_direct_voice);
                    voices ^= bit;
                }
                bit <<= 1;
                voice++;
            } while (voices);
        }
        seq_song_paused_voices = seq_song_active_voices;
        seq_song_active_voices = 0;
    }
    D_80062FF8 |= 1;
}

/* 9A: resumes the music. Its voices rewrite their volume and sustain. */
void snd_cmd_9A(SoundCommand *cmd)
{
    unsigned int voices = seq_song_paused_voices;
    SeqChannel *ch;
    unsigned int bit;

    if (voices) {
        bit = 1;
        ch = seq_channels;
        do {
            if (voices & bit) {
                voices ^= bit;
                ch->voice.update |= UPDATE_VOLUME | UPDATE_SUSTAIN_MODE | UPDATE_SUSTAIN_RATE;
            }
            bit <<= 1;
            ch++;
        } while (voices);
        seq_song_active_voices = seq_song_paused_voices;
        seq_song_paused_voices = 0;
        snd_apply_noise_voices();
        snd_apply_reverb_voices();
        snd_apply_fm_voices();
    }
    D_80062FF8 &= ~1;
}

/* 9D: pauses the effects, all but voices 22/23 while channel 54 plays a kind 2 effect. */
void snd_cmd_9D(SoundCommand *cmd)
{
    unsigned int *held = &effect_voices;
    unsigned int all = *held;
    unsigned int voices = all;
    unsigned int bit;
    unsigned int voice;

    if (voices) {
        bit = 0x10000;
        if (seq_channels[54].is_effect == 2)
            voices &= ~0xC00000;
        effect_paused_voices = voices;
        *held = voices ^ all;
        snd_direct_voice.volume[1] = 0;
        snd_direct_voice.volume[0] = 0;
        snd_direct_voice.sustain_rate = 0x7F;
        voice = 16;
        while (voices) {
            if (voices & bit) {
                snd_direct_voice_update = UPDATE_VOLUME | UPDATE_SUSTAIN_MODE | UPDATE_SUSTAIN_RATE;
                snd_update_voice(voice, &snd_direct_voice);
                voices ^= bit;
            }
            bit <<= 1;
            voice++;
        }
    }
    D_80062FF8 |= 2;
}

/* 9C: resumes the effects. */
void snd_cmd_9C(SoundCommand *cmd)
{
    unsigned int voices = effect_paused_voices;
    SeqChannel *ch;
    unsigned int bit;

    if (voices) {
        bit = 0x10000;
        ch = &seq_channels[48];
        do {
            if (voices & bit) {
                voices ^= bit;
                ch->voice.update |= UPDATE_VOLUME | UPDATE_SUSTAIN_MODE | UPDATE_SUSTAIN_RATE;
            }
            bit <<= 1;
            ch++;
        } while (voices);
        effect_voices = effect_paused_voices;
        effect_paused_voices = 0;
        snd_apply_noise_voices();
        snd_apply_reverb_voices();
        snd_apply_fm_voices();
    }
    D_80062FF8 &= ~2;
}

void snd_cmd_E0(SoundCommand *cmd)
{
    D_80062F70 = cmd->arg[0] & 0x7F;
    seq_song_flags |= SONG_UPDATE_REVERB;
}

/* E4 n: stores n (a byte) and sets flag 0x10 at 0x80062FF8 when it's non-zero; the reverb is updated. */
void snd_cmd_E4(SoundCommand *cmd)
{
    unsigned short n = (unsigned char)cmd->arg[0];

    D_80062FB8 = n;
    if (n)
        D_80062FF8 |= 0x10;
    else
        D_80062FF8 &= ~0x10;
    snd_apply_reverb_voices();
    seq_song_flags |= SONG_UPDATE_REVERB;
}

void snd_cmd_F2(SoundCommand *cmd)
{
    saved_songs[0].song_id = 0;
}

void snd_cmd_F3(SoundCommand *cmd)
{
    saved_songs[1].song_id = 0;
}

/* F4: sets the effects aside (their 8 channels and state, in channels 24-31 and after) and stops them. F5 brings
 * them back. */
void snd_cmd_F4(SoundCommand *cmd)
{
    unsigned int *src;
    unsigned int *dst;
    unsigned int *saved;
    unsigned short n;

    snd_keep_looping_effects();
    src = (unsigned int *)&seq_channels[48];
    dst = (unsigned int *)&seq_channels[24];
    n = sizeof(SeqChannel) * 8 / 4;
    do {
        n--;
        *dst++ = *src++;
    } while (n);
    n = 12;
    src = &effect_voices;
    saved = dst;
    do {
        n--;
        *dst++ = *src++;
    } while (n);
    if (seq_channels[54].is_effect == 2) {
        saved[0] &= ~0xC00000;
        saved[1] &= ~0xC00000;
    }
    D_80062FF8 |= 0x100;
    snd_stop_effects();
    cmd->arg[0] = 0x7F;
    snd_cmd_B8(cmd);
    cmd->arg[0] = 0;
    snd_cmd_BC(cmd);
}

void snd_cmd_F5(SoundCommand *cmd)
{
    unsigned int *src;
    unsigned int *dst;
    unsigned int held;
    unsigned int *state;
    unsigned short n;

    if (!(D_80062FF8 & 0x100))
        return;
    src = (unsigned int *)&seq_channels[24];
    dst = (unsigned int *)&seq_channels[48];
    n = sizeof(SeqChannel) * 8 / 4;
    do {
        n--;
        *dst++ = *src++;
    } while (n);
    state = &effect_voices;
    held = *state;
    n = 12;
    dst = state;
    do {
        n--;
        *dst++ = *src++;
    } while (n);
    effect_key_off_voices = held & ~effect_voices;
    D_80062FF8 &= ~0x100;
    snd_apply_noise_voices();
    snd_apply_reverb_voices();
    snd_apply_fm_voices();
    seq_song_flags |= SONG_UPDATE_NOISE;
}

/* F8/F9: after func_8002CFC0, the stream's voices leave the effects, and their reverb goes off (F8) or on (F9).
 * The game reaches effect_voices through a pointer here. */
void snd_cmd_F8(SoundCommand *cmd)
{
    unsigned int *voices = &effect_voices;

    func_8002CFC0(cmd);
    *voices &= ~snd_stream_voices;
    effect_reverb_voices &= ~snd_stream_voices;
    snd_apply_reverb_voices();
}

void snd_cmd_F9(SoundCommand *cmd)
{
    unsigned int *voices = &effect_voices;

    func_8002CFC0(cmd);
    *voices &= ~snd_stream_voices;
    effect_reverb_voices |= snd_stream_voices;
    snd_apply_reverb_voices();
}

void snd_cmd_FA(SoundCommand *cmd)
{
    snd_stream_stop();
}

void func_8002CF98(void) {
}

void func_8002CFA0(void)
{
    func_80038FEC(0);
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002CFC0);

/* Sets up voice 16 for the mono stream: the buffer at 0x77000 looping on itself, at snd_stream_volume's volume split by
 * snd_stream_pan (0 left to 127 right). */
void snd_stream_voice_mono(void)
{
    snd_direct_voice_update = UPDATE_ALL;
    snd_direct_voice_attack_rate = 0;
    snd_direct_voice_sample = 0x77000;
    snd_direct_voice_loop = 0x77000;
    snd_direct_voice_decay_rate = 0xF;
    snd_direct_voice_sustain_level = 0xF;
    snd_direct_voice_sustain_rate = 0x7F;
    snd_direct_voice_release_rate = 6;
    snd_direct_voice_attack_mode = 1;
    snd_direct_voice_sustain_mode = 3;
    snd_direct_voice_release_mode = 3;
    snd_direct_voice_volume_left = ((snd_stream_pan ^ 0x7F) * snd_stream_volume) >> 7;
    snd_direct_voice_pitch = snd_stream_pitch;
    snd_direct_voice_volume_right = (snd_stream_volume * snd_stream_pan) >> 7;
    snd_update_voice(16, &snd_direct_voice);
}

/* Sets up voices 16/17 for the stereo stream: left from 0x77000 and right from 0x77800, half volume each. */
void snd_stream_voices_stereo(void)
{
    snd_direct_voice_update = UPDATE_ALL;
    snd_direct_voice_volume_right = 0;
    snd_direct_voice_attack_rate = 0;
    snd_direct_voice_sample = 0x77000;
    snd_direct_voice_loop = 0x78000;
    snd_direct_voice_decay_rate = 0xF;
    snd_direct_voice_sustain_level = 0xF;
    snd_direct_voice_sustain_rate = 0x7F;
    snd_direct_voice_release_rate = 6;
    snd_direct_voice_attack_mode = 1;
    snd_direct_voice_sustain_mode = 3;
    snd_direct_voice_release_mode = 3;
    snd_direct_voice_volume_left = snd_stream_volume >> 1;
    snd_direct_voice_pitch = snd_stream_pitch;
    snd_update_voice(16, &snd_direct_voice);
    snd_direct_voice_update = UPDATE_ALL;
    snd_direct_voice_volume_left = 0;
    snd_direct_voice_sample = 0x77800;
    snd_direct_voice_loop = 0x78800;
    snd_direct_voice_volume_right = snd_stream_volume >> 1;
    snd_update_voice(17, &snd_direct_voice);
}

/* Starts the mono stream: the first IRQ at 0x78000 when there is data, then its voices on. */
void snd_stream_play_mono(void)
{
    func_80038FEC(0);
    if (snd_stream_left) {
        func_80038A84(0);
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_fill_mono_a);
        func_80038A84(1);
    }
    func_80038C6C(1, snd_stream_voices);
    effect_key_off_voices &= ~snd_stream_voices;
}

/* snd_stream_play_mono again, with snd_stream_fill_stereo_a as the IRQ callback. */
void snd_stream_play_stereo(void)
{
    func_80038FEC(0);
    if (snd_stream_left) {
        func_80038A84(0);
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_fill_stereo_a);
        func_80038A84(1);
    }
    func_80038C6C(1, snd_stream_voices);
    effect_key_off_voices &= ~snd_stream_voices;
}

/* The mono stream's IRQ at 0x78000: copies the next 4 KB into the first half (0x77000) and arms the IRQ there for
 * snd_stream_fill_mono_b. At the end it loops (snd_stream_loop, snd_stream_loop_size bytes) or stops. */
void snd_stream_fill_mono_a(void)
{
    if (!snd_stream_left)
        return;
    func_80038F64(0x77000);
    func_80038F04(snd_stream_pos, 0x1000);
    func_80038A84(0);
    if (snd_stream_left > 0x1000) {
        SpuSetIRQAddr(0x77000);
        SpuSetIRQCallback(snd_stream_fill_mono_b);
        func_80038A84(1);
        snd_stream_left -= 0x1000;
        snd_stream_pos += 0x1000;
    } else if (snd_stream_loop) {
        SpuSetIRQAddr(0x77000);
        SpuSetIRQCallback(snd_stream_fill_mono_b);
        func_80038A84(1);
        snd_stream_pos = snd_stream_loop;
        snd_stream_left = snd_stream_loop_size;
    } else {
        snd_stream_left = 0;
        SpuSetIRQAddr(0x77000);
        SpuSetIRQCallback(snd_stream_stop);
        func_80038A84(1);
    }
}

/* The other half (0x78000). */
void snd_stream_fill_mono_b(void)
{
    if (!snd_stream_left)
        return;
    func_80038F64(0x78000);
    func_80038F04(snd_stream_pos, 0x1000);
    func_80038A84(0);
    if (snd_stream_left > 0x1000) {
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_fill_mono_a);
        func_80038A84(1);
        snd_stream_left -= 0x1000;
        snd_stream_pos += 0x1000;
    } else if (snd_stream_loop) {
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_fill_mono_a);
        func_80038A84(1);
        snd_stream_pos = snd_stream_loop;
        snd_stream_left = snd_stream_loop_size;
    } else {
        snd_stream_left = 0;
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_stop);
        func_80038A84(1);
    }
}

/* The stereo stream's: the same 4 KB block is 2 KB left (voice 16) and 2 KB right (voice 17), and each refill
 * points the voices' loop at it. */
void snd_stream_fill_stereo_a(void)
{
    if (!snd_stream_left)
        return;
    func_80038F64(0x77000);
    func_80038F04(snd_stream_pos, 0x1000);
    func_80038A84(0);
    func_800396C0(16, 0x77000);
    func_800396C0(17, 0x77800);
    if (snd_stream_left > 0x1000) {
        SpuSetIRQAddr(0x77000);
        SpuSetIRQCallback(snd_stream_fill_stereo_b);
        snd_stream_left -= 0x1000;
        snd_stream_pos += 0x1000;
    } else if (snd_stream_loop) {
        SpuSetIRQAddr(0x77000);
        SpuSetIRQCallback(snd_stream_fill_stereo_b);
        snd_stream_pos = snd_stream_loop;
        snd_stream_left = snd_stream_loop_size;
    } else {
        snd_stream_left = 0;
        SpuSetIRQAddr(0x77000);
        SpuSetIRQCallback(snd_stream_stop);
    }
    func_80038A84(1);
}

/* The other half (0x78000/0x78800). */
void snd_stream_fill_stereo_b(void)
{
    if (!snd_stream_left)
        return;
    func_80038F64(0x78000);
    func_80038F04(snd_stream_pos, 0x1000);
    func_80038A84(0);
    func_800396C0(16, 0x78000);
    func_800396C0(17, 0x78800);
    if (snd_stream_left > 0x1000) {
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_fill_stereo_a);
        snd_stream_left -= 0x1000;
        snd_stream_pos += 0x1000;
    } else if (snd_stream_loop) {
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_fill_stereo_a);
        snd_stream_pos = snd_stream_loop;
        snd_stream_left = snd_stream_loop_size;
    } else {
        snd_stream_left = 0;
        SpuSetIRQAddr(0x78000);
        SpuSetIRQCallback(snd_stream_stop);
    }
    func_80038A84(1);
}

/* The next free command in the queue. */
void snd_queue_next(SoundCommand **cmd)
{
    *cmd = snd_queue;
    *cmd += snd_queue_count;
    snd_queue_count++;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002DA7C);

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_run_command);

/* Runs every queued sound command, unless the queue is held. */
void snd_flush_queue(void)
{
    SoundCommand *cmd;

    if (D_80062F8C)
        return;
    cmd = snd_queue;
    while (snd_queue_count) {
        snd_command_handlers[(unsigned char)cmd->id](cmd);
        snd_queue_count--;
        cmd++;
    }
}

INCLUDE_ASM("asm/nonmatchings/main/sound", snd_update_voice);

/* The CD audio's volume on both sides, with no reverb. */
void snd_apply_cd_volume(void)
{
    SpuCommonAttr *attr = &snd_common_attr;

    attr->mask = 0x1C0;
    attr->cd_reverb = 0;
    attr->cd_volume[1] = (unsigned int)snd_cd_volume >> 16;
    attr->cd_volume[0] = (unsigned int)snd_cd_volume >> 16;
    func_80039034(attr);
}

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_channel_slides);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002E954);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_channel_lfo);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002F24C);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_layer_voice_update);

INCLUDE_ASM("asm/nonmatchings/main/sound", func_8002F848);

/* Adds voices to *mask, and with them each one's spare voice (F4 layer or F8 overlap) when it's in spares. */
void snd_add_spare_voices(SeqChannel *ch, unsigned int *mask, unsigned int voices, unsigned int spares)
{
    unsigned int bit = 1;
    unsigned int spare;
    unsigned short layer;

    *mask |= voices;
    if (!voices)
        return;
    do {
        if (voices & bit) {
            if (ch->flags & CHANNEL_LAYER) {
                layer = ch->layer_voice;
                if (ch->layer_voice >= 24)
                    layer += 0xFFE8;
                spare = 1 << layer;
            } else {
                if (!(ch->flags & CHANNEL_OVERLAP))
                    goto next;
                spare = 1 << ch->overlap_voice;
            }
            if (spares & spare)
                *mask |= spare;
        next:
            voices ^= bit;
        }
        bit <<= 1;
        ch++;
    } while (voices);
}

/* Keys off the voices both songs and the effects have let go of (key_off_voices and effect_key_off_voices), spare voices included. */
void snd_key_off_voices(void)
{
    unsigned int mask = 0;
    unsigned int voices;

    if (seq_song2.active_voices) {
        voices = seq_song2.key_off_voices & D_80062F68 & ~(effect_voices | snd_stream_voices);
        if (voices)
            snd_add_spare_voices(&seq_channels[24], &mask, voices, ~(effect_voices | snd_stream_voices));
        seq_song2.key_off_voices = 0;
    }
    if (seq_song_active_voices) {
        voices = ~(D_80062F68 | effect_voices | snd_stream_voices) & seq_song_key_off_voices;
        if (voices)
            snd_add_spare_voices(seq_channels, &mask, voices, ~(D_80062F68 | effect_voices | snd_stream_voices));
        seq_song_key_off_voices = 0;
    }
    mask |= effect_key_off_voices;
    effect_key_off_voices = 0;
    if (mask)
        func_80038C6C(0, mask);
}

/* Hands the noise voices to the SPU: the songs' (spare voices included, not the ones effects hold) and the effects'. */
void snd_apply_noise_voices(void)
{
    unsigned int *held = &effect_voices;
    unsigned int mask = 0;
    unsigned int voices;

    voices = seq_song2.noise_voices & D_80062F68 & ~(*held | snd_stream_voices);
    if (voices)
        snd_add_spare_voices(&seq_channels[24], &mask, voices, ~(*held | snd_stream_voices));
    voices = ~(D_80062F68 | *held | snd_stream_voices) & seq_song.noise_voices;
    if (voices)
        snd_add_spare_voices(seq_channels, &mask, voices, ~(D_80062F68 | *held | snd_stream_voices));
    mask |= effect_noise_voices;
    func_80037964(1, mask);
    mask ^= 0xFFFFFF;
    func_80037964(0, mask);
}

/* The same for reverb. E4's flag puts every voice in it. */
void snd_apply_reverb_voices(void)
{
    unsigned int *held = &effect_voices;
    unsigned int mask = 0;
    unsigned int voices;

    voices = seq_song2.reverb_voices & D_80062F68 & ~(*held | snd_stream_voices);
    if (voices)
        snd_add_spare_voices(&seq_channels[24], &mask, voices, ~(*held | snd_stream_voices));
    if (D_80062FF8 & 0x10) {
        mask = 0xFFFFFF;
    } else {
        voices = ~(D_80062F68 | *held | snd_stream_voices) & seq_song.reverb_voices;
        if (voices)
            snd_add_spare_voices(seq_channels, &mask, voices, ~(D_80062F68 | *held | snd_stream_voices));
    }
    mask |= effect_reverb_voices;
    func_800388C4(1, mask);
    mask ^= 0xFFFFFF;
    func_800388C4(0, mask);
}

/* The same for FM. */
void snd_apply_fm_voices(void)
{
    unsigned int *held = &effect_voices;
    unsigned int mask = 0;
    unsigned int voices;

    voices = seq_song2.fm_voices & D_80062F68 & ~(*held | snd_stream_voices);
    if (voices)
        snd_add_spare_voices(&seq_channels[24], &mask, voices, ~(*held | snd_stream_voices));
    voices = ~(D_80062F68 | *held | snd_stream_voices) & seq_song.fm_voices;
    if (voices)
        snd_add_spare_voices(seq_channels, &mask, voices, ~(D_80062F68 | *held | snd_stream_voices));
    mask |= effect_fm_voices;
    func_80039010(1, mask);
    mask ^= 0xFFFFFF;
    func_80039010(0, mask);
}

/* Root counter 2's interrupt: runs the sequencer ticks due since the last call (scanlines / 66, 1-8 of them, doubled
 * by 0x80062FF8 bit 2), and keeps how long the last four calls took (counter 2 units) and their total. */
unsigned int snd_timer_tick(void)
{
    int start = GetRCnt(0xF2000002);
    unsigned int count = VSync(1);
    unsigned short ticks;

    if (count < snd_timer_last_count)
        snd_timer_last_count = 0;
    ticks = (count - snd_timer_last_count) / 66;
    if (ticks == 0 || ticks > 8)
        ticks = 1;
    snd_timer_last_count = count;
    count = ticks;
    if (D_80062FF8 & 4)
        ticks <<= 1;
    while (ticks) {
        ticks--;
        seq_tick();
    }
    start = GetRCnt(0xF2000002) - start;
    if (start <= 0)
        start += 0x43D1;
    snd_tick_time[0] = snd_tick_time[1];
    snd_tick_time[1] = snd_tick_time[2];
    snd_tick_time[2] = snd_tick_time[3];
    snd_tick_time[3] = start;
    snd_tick_time_total = snd_tick_time[0] + snd_tick_time[1] + snd_tick_time[2] + snd_tick_time[3];
    return count;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", func_80030380);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_tick);

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_step_channel);

/* A program change: the instrument's sample, loop and whole envelope. */
void load_channel_instrument(SeqChannel *ch, unsigned short instrument)
{
    Instrument *inst;

    ch->instrument = instrument;
    inst = &instrument_table[instrument];
    ch->voice.sample = inst->sample;
    ch->voice.loop = inst->loop;
    ch->voice.attack_mode = inst->adsr[5];
    ch->voice.sustain_mode = inst->adsr[6];
    ch->voice.release_mode = inst->adsr[7];
    ch->voice.attack_rate = inst->adsr[0];
    ch->voice.decay_rate = inst->adsr[1];
    ch->voice.sustain_level = inst->adsr[2];
    ch->voice.sustain_rate = inst->adsr[3];
    ch->voice.release_rate = inst->adsr[4];
    ch->voice.update |= UPDATE_SAMPLE | UPDATE_ENVELOPE | UPDATE_LOOP;
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
    ch->voice.update |= UPDATE_VOLUME;
}

void seq_cmd_volume(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->volume = *(signed char *)ch->pos++ << 23;
    ch->volume_slide_ticks = 0;
    ch->voice.update |= UPDATE_VOLUME;
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
        ch->voice.update |= UPDATE_VOLUME;
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
    ch->voice.update |= UPDATE_VOLUME;
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
        ch->voice.update |= UPDATE_PITCH;
        ch->note_pitch = ch->note_pitch * instrument_table[n].pitch[0] / instrument_table[ch->instrument].pitch[0];
    }
    if (ch->flags & CHANNEL_OVERLAP) {
        inst = &instrument_table[n];
        ch->instrument = n;
        ch->voice.sample = inst->sample;
        ch->voice.loop = inst->loop;
        ch->voice.attack_rate = inst->adsr[0];
        ch->voice.decay_rate = inst->adsr[1];
        ch->voice.sustain_level = inst->adsr[2];
        ch->voice.sustain_rate = inst->adsr[3];
        ch->voice.attack_mode = inst->adsr[5];
        ch->voice.sustain_mode = inst->adsr[6];
        ch->voice.update |= UPDATE_SAMPLE | UPDATE_ATTACK_MODE | UPDATE_SUSTAIN_MODE | UPDATE_ATTACK_RATE |
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
        ch->voice.update |= UPDATE_PITCH;
        ch->note_pitch = ch->note_pitch * inst->pitch[0] / instrument_table[ch->instrument].pitch[0];
    }
    ch->instrument = n;
    ch->voice.sample = 0x76FE0;
    ch->voice.loop = inst->loop;
    ch->voice.attack_rate = inst->adsr[0];
    ch->voice.decay_rate = inst->adsr[1];
    ch->voice.sustain_level = inst->adsr[2];
    ch->voice.sustain_rate = inst->adsr[3];
    ch->voice.attack_mode = inst->adsr[5];
    ch->voice.sustain_mode = inst->adsr[6];
    if (ch->flags & CHANNEL_OVERLAP) {
        ch->voice.update |= UPDATE_SAMPLE | UPDATE_ATTACK_MODE | UPDATE_SUSTAIN_MODE | UPDATE_ATTACK_RATE |
                            UPDATE_DECAY_RATE | UPDATE_SUSTAIN_RATE | UPDATE_SUSTAIN_LEVEL | UPDATE_LOOP;
    } else {
        ch->voice.release_rate = inst->adsr[4];
        ch->voice.release_mode = inst->adsr[7];
        ch->voice.update |= UPDATE_SAMPLE | UPDATE_ENVELOPE | UPDATE_LOOP;
    }
}

/* B3: the instrument's envelope back, on the layer voice too. */
void seq_cmd_envelope_reset(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    Instrument *inst = &instrument_table[ch->instrument];
    SeqChannel *layer;

    ch->voice.attack_rate = inst->adsr[0];
    ch->voice.decay_rate = inst->adsr[1];
    ch->voice.sustain_level = inst->adsr[2];
    ch->voice.sustain_rate = inst->adsr[3];
    ch->voice.release_rate = inst->adsr[4];
    ch->voice.attack_mode = inst->adsr[5];
    ch->voice.sustain_mode = inst->adsr[6];
    ch->voice.release_mode = inst->adsr[7];
    ch->voice.update |= UPDATE_ENVELOPE;
    if (ch->flags & CHANNEL_LAYER) {
        layer = &seq_channels[ch->layer_voice];
        layer->voice.attack_rate = ch->voice.attack_rate;
        layer->voice.decay_rate = ch->voice.decay_rate;
        layer->voice.sustain_level = ch->voice.sustain_level;
        layer->voice.sustain_rate = ch->voice.sustain_rate;
        layer->voice.release_rate = ch->voice.release_rate;
        layer->voice.attack_mode = ch->voice.attack_mode;
        layer->voice.sustain_mode = ch->voice.sustain_mode;
        layer->voice.release_mode = ch->voice.release_mode;
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
    ch->voice.update |= UPDATE_PITCH;
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
    ch->voice.update |= UPDATE_VOLUME;
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
    ch->voice.update |= UPDATE_VOLUME;
}

void seq_cmd_noise_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->noise_voices |= voice;
    else
        effect_noise_voices |= voice;
    seq_song_flags |= SONG_UPDATE_NOISE;
    snd_apply_noise_voices();
}

void seq_cmd_noise_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->noise_voices &= ~voice;
    else
        effect_noise_voices &= ~voice;
    seq_song_flags |= SONG_UPDATE_NOISE;
    snd_apply_noise_voices();
    ch->noise_ticks = 0;
}

/* An effect's first channel sits on an even voice (0x555555), which never takes FM. */
void seq_cmd_fm_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->fm_voices |= voice;
    else if (!(voice & 0x555555))
        effect_fm_voices |= voice;
    snd_apply_fm_voices();
}

void seq_cmd_fm_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->fm_voices &= ~voice;
    else
        effect_fm_voices &= ~voice;
    snd_apply_fm_voices();
    ch->fm_ticks = 0;
}

void seq_cmd_reverb_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->reverb_voices |= voice;
    else
        effect_reverb_voices |= voice;
    snd_apply_reverb_voices();
}

void seq_cmd_reverb_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    if (!ch->is_effect)
        song->reverb_voices &= ~voice;
    else
        effect_reverb_voices &= ~voice;
    snd_apply_reverb_voices();
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
    ch->voice.attack_rate = *ch->pos++;
    ch->voice.update |= UPDATE_ATTACK_RATE | UPDATE_ATTACK_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.attack_rate = ch->voice.attack_rate;
}

void seq_cmd_decay_rate(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->voice.decay_rate = *ch->pos++;
    ch->voice.update |= UPDATE_DECAY_RATE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.decay_rate = ch->voice.decay_rate;
}

void seq_cmd_sustain_level(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->voice.sustain_level = *ch->pos++;
    ch->voice.update |= UPDATE_SUSTAIN_LEVEL;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.sustain_level = ch->voice.sustain_level;
}

void seq_cmd_sustain_rate(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->voice.sustain_rate = *ch->pos++;
    ch->voice.update |= UPDATE_SUSTAIN_RATE | UPDATE_SUSTAIN_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.sustain_rate = ch->voice.sustain_rate;
}

void seq_cmd_release_rate(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->voice.release_rate = *ch->pos++;
    ch->voice.update |= UPDATE_RELEASE_RATE | UPDATE_RELEASE_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.release_rate = ch->voice.release_rate;
}

void seq_cmd_attack_mode(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->voice.attack_mode = *ch->pos++;
    ch->voice.update |= UPDATE_ATTACK_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.attack_mode = ch->voice.attack_mode;
}

void seq_cmd_sustain_mode(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->voice.sustain_mode = *ch->pos++;
    ch->voice.update |= UPDATE_SUSTAIN_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.sustain_mode = ch->voice.sustain_mode;
}

void seq_cmd_release_mode(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    ch->voice.release_mode = *ch->pos++;
    ch->voice.update |= UPDATE_RELEASE_MODE;
    if (ch->flags & CHANNEL_LAYER)
        seq_channels[ch->layer_voice].voice.release_mode = ch->voice.release_mode;
}

INCLUDE_ASM("asm/nonmatchings/main/sound", seq_cmd_overlap_on);

/* F9: frees the second voice, and the release rate goes back to the instrument's. */
void seq_cmd_overlap_off(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->overlap_voices &= ~(1 << ch->overlap_voice);
    ch->flags &= ~CHANNEL_OVERLAP;
    ch->voice.release_rate = instrument_table[ch->instrument].adsr[4];
    ch->voice.update |= UPDATE_RELEASE_RATE | UPDATE_RELEASE_MODE;
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
