#include "common.h"
#include "sound.h"

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
    ch->bend_offset = 0;
    ch->bend_semitones = 0;
    ch->fixed_length = 0;
    ch->last_length = 0;
    ch->bend_count = 0;
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

/* Starts the song in song_data: a word with its channel mask, then a 16-bit offset per channel (from after the
 * offset) to its sequence. Every channel starts on instrument 20, near full volume, centred, and the song's tempo,
 * reverb, meter and voice masks start over. While the music is paused (9B), the new voices go straight to the
 * paused set. */
void snd_start_song(void)
{
    unsigned char *data = (unsigned char *)song_data;
    SeqChannel *ch;
    unsigned int voices;
    unsigned int bit;
    unsigned short offset;

    voices = *(unsigned int *)data & 0xFFFFFF;
    data += 4;
    ch = seq_channels;
    seq_song_key_off_voices |= 0xFFFFFF;
    seq_song_active_voices = voices;
    bit = 1;
    while (voices) {
        if (voices & bit) {
            offset = *(unsigned short *)data;
            data += 2;
            voices ^= bit;
            ch->pos = data + offset;
            ch->gate = 0x103;
            ch->volume_scale = 0x7F;
            load_channel_instrument(ch, 20);
            ch->volume = 0x3FFF0000;
            ch->balance = 0x4000;
            ch->pan = 0x4000;
            ch->drum_map = (unsigned char *)song_data;
            ch->detune = 0;
            ch->transpose = 0;
            ch->bend_offset = 0;
            ch->bend_semitones = 0;
            ch->bend_count = 0;
            ch->glide_ticks = 0;
            ch->fixed_length = 0;
            ch->last_length = 0;
            ch->balance_slide_ticks = 0;
            ch->volume_slide_ticks = 0;
            ch->pan_slide_ticks = 0;
            ch->glide_ticks = 0;
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
        ch++;
        bit <<= 1;
    }
    seq_song.tempo = 0xFFFF0000;
    seq_song.tempo_phase = 1;
    seq_song.tempo_slide_ticks = 0;
    seq_song.reverb_depth = 0;
    seq_song.reverb_slide_ticks = 0;
    seq_song.reverb_slide_step = 0;
    seq_song_flags = 0;
    seq_song.beat_tick = 0;
    seq_song.beat_ticks = 0;
    seq_song.bar_beat = 0;
    seq_song.bar_number = 0;
    seq_song.noise_voices = 0;
    seq_song.reverb_voices = 0;
    seq_song.fm_voices = 0;
    seq_song_branch_level = 0;
    seq_song.branch_taken = 0;
    seq_song.pause_pending = 0;
    seq_song_update_voices = 0;
    seq_song_key_on_voices = 0;
    seq_song_overlap_voices = 0;
    seq_song_layer_voices = 0;
    if (D_80062FF8 & 1) {
        seq_song_paused_voices = seq_song_active_voices;
        seq_song_active_voices = 0;
    }
    snd_apply_noise_voices();
    snd_apply_reverb_voices();
    snd_apply_fm_voices();
}

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
    seq_song_key_on_voices = 0;
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
        seq_song_key_on_voices = 0;
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
    seq_song2.key_on_voices = 0;
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
        effect_key_on_voices &= 0xC00000;
        effect_update_voices &= 0xC00000;
        effect_key_off_voices &= ~0xC00000 & effect_voices;
    } else {
        effect_key_on_voices = 0;
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
        effect_key_on_voices &= ~mask;
        effect_update_voices &= ~mask;
        effect_key_off_voices |= mask;
        break;
    case 2:
        effect_key_on_voices &= ~0x3C0000;
        effect_update_voices &= ~0x3C0000;
        effect_key_off_voices |= 0x3C0000;
        break;
    case 3:
        effect_key_on_voices &= ~0x3F0000;
        effect_update_voices &= ~0x3F0000;
        effect_key_off_voices |= 0x3F0000;
        break;
    case 4:
        effect_key_on_voices &= ~0xFF0000;
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
    seq_song_key_on_voices = seq_song_update_voices;
    ch = seq_channels;
    do {
        if (voices & bit) {
            if (SpuGetKeyStatus(bit) == 1) {
                if (ch->flags & CHANNEL_OVERLAP)
                    ch->flags |= 0x400;
                seq_song_key_on_voices |= bit;
            } else if ((ch->flags & 0x600) == 0x600
                       && (~(effect_voices | snd_stream_voices) & (spare = 1 << ch->overlap_voice))
                       && SpuGetKeyStatus(spare) == 1) {
                seq_song_key_on_voices |= bit;
            } else {
                seq_song_key_on_voices &= ~bit;
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
    effect_key_on_voices = effect_update_voices;
    do {
        if (voices & bit) {
            if (seq_stream_end(ch) == 0xCA) {
                if (SpuGetKeyStatus(bit) == 1) {
                    effect_key_on_voices |= bit;
                } else {
                    effect_key_on_voices &= ~bit;
                    effect_update_voices &= ~bit;
                }
                ch->voice.update = UPDATE_ALL;
            } else if (ch->is_effect != 2) {
                *held &= ~bit;
                effect_key_on_voices &= ~bit;
                effect_update_voices &= ~bit;
            }
            voices ^= bit;
        }
        ch++;
        bit <<= 1;
    } while (voices);
}

/* Resumes the song in a save slot: its state (the beat's tick count 2 back) and channels come back, the channels it
 * doesn't hold step into the end stub, and its voices rewrite everything. Unless a slide of 0x80062F5C or the fade is
 * running, that value restarts at 10 and slides back over 60 ticks. The slot is emptied. */
void snd_resume_song(int slot)
{
    unsigned short s = slot;
    unsigned int *src;
    unsigned int *dst;
    unsigned short n;
    unsigned int voices;
    unsigned int bit;
    SeqChannel *ch;

    seq_song.active_voices = saved_songs[s].active_voices;
    seq_song.key_on_voices = saved_songs[s].key_on_voices;
    seq_song.update_voices = saved_songs[s].update_voices;
    seq_song.tempo = saved_songs[s].tempo;
    seq_song.tempo_slide_step = saved_songs[s].tempo_slide_step;
    seq_song.tempo_slide_ticks = saved_songs[s].tempo_slide_ticks;
    seq_song.tempo_phase = saved_songs[s].tempo_phase;
    seq_song.layer_voices = saved_songs[s].layer_voices;
    seq_song.overlap_voices = saved_songs[s].overlap_voices;
    seq_song.song_id = saved_songs[s].song_id;
    seq_song.branch_taken = saved_songs[s].branch_taken;
    seq_song.branch_level = saved_songs[s].branch_level;
    seq_song.reverb_depth = saved_songs[s].reverb_depth;
    seq_song.reverb_slide_step = saved_songs[s].reverb_slide_step;
    seq_song.reverb_slide_ticks = saved_songs[s].reverb_slide_ticks;
    seq_song.noise_clock = saved_songs[s].noise_clock;
    seq_song.noise_voices = saved_songs[s].noise_voices;
    seq_song.reverb_voices = saved_songs[s].reverb_voices;
    seq_song.fm_voices = saved_songs[s].fm_voices;
    seq_song.pause_pending = saved_songs[s].pause_pending;
    seq_song.flags = saved_songs[s].flags;
    seq_song.beats_per_bar = saved_songs[s].beats_per_bar;
    seq_song.bar_beat = saved_songs[s].bar_beat;
    seq_song.beat_ticks = saved_songs[s].beat_ticks;
    seq_song.beat_tick = saved_songs[s].beat_tick - 2;
    seq_song.bar_number = saved_songs[s].bar_number;
    seq_song.flags |= SONG_UPDATE_REVERB;
    src = (unsigned int *)saved_song_channels[1];
    if (!s)
        src = (unsigned int *)saved_song_channels[0];
    n = sizeof(SeqChannel) * 24 / 4;
    dst = (unsigned int *)seq_channels;
    do {
        n--;
        *dst++ = *src++;
    } while (n);
    voices = seq_song_active_voices;
    if (voices) {
        ch = seq_channels;
        bit = 1;
        for (n = 24; n; n--, ch++, bit <<= 1) {
            if (!(voices & bit)) {
                ch->gate = 0x204;
                ch->pos = D_80049C40;
            }
        }
    }
    voices |= seq_song_layer_voices;
    seq_song_key_off_voices = ~seq_song_key_on_voices & 0xFFFFFF;
    ch = seq_channels;
    bit = 1;
    while (voices) {
        if (voices & bit) {
            voices ^= bit;
            ch->gate += 0x202;
            ch->voice.update |= UPDATE_ALL;
        }
        ch++;
        bit <<= 1;
    }
    snd_apply_noise_voices();
    snd_apply_reverb_voices();
    snd_apply_fm_voices();
    if (!snd_master_volume_ticks && !snd_channel_fades[0].ticks) {
        snd_master_volume_ticks = 60;
        snd_master_volume_step = (snd_master_volume - 0xA0000) / 60;
        snd_master_volume = 0xA0000;
    }
    saved_songs[s].song_id = 0;
    if (D_80062FF8 & 1) {
        seq_song_paused_voices = seq_song_active_voices;
        seq_song_active_voices = 0;
    }
}

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
    snd_load_song((unsigned int *)cmd->arg[0], cmd->arg[1]);
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

    snd_load_song((unsigned int *)cmd->arg[0], cmd->arg[1]);
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
    snd_load_song((unsigned int *)cmd->arg[0], cmd->arg[1]);
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
    snd_master_volume = (cmd->arg[0] & 0x7F) << 16;
    snd_master_volume_ticks = 0;
    snd_refresh_song_volumes();
}

void snd_cmd_C1(SoundCommand *cmd)
{
    int ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    snd_master_volume_step = (((cmd->arg[1] & 0x7F) << 16) - snd_master_volume) / ticks;
    snd_master_volume_ticks = ticks;
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
    snd_master_volume_step = (to - from) / ticks;
    snd_master_volume_ticks = ticks;
    snd_master_volume = from;
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

/* D0 v: the tempo scale = v (signed) << 16. D1 t v slides it over t ticks (0 = 1). D4/D5 do the same for the pitch
 * scale. */
void snd_cmd_D0(SoundCommand *cmd)
{
    snd_tempo_scale = (signed char)cmd->arg[0] << 16;
    snd_tempo_scale_ticks = 0;
}

void snd_cmd_D1(SoundCommand *cmd)
{
    int ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    snd_tempo_scale_step = (((signed char)cmd->arg[1] << 16) - snd_tempo_scale) / ticks;
    snd_tempo_scale_ticks = ticks;
}

/* D2 a t b: D0's value from a to b over t ticks. D6: the same for D4's. */
void snd_cmd_D2(SoundCommand *cmd)
{
    int ticks = 1;
    int from = (signed char)cmd->arg[0] << 16;

    snd_tempo_scale = from;
    if (cmd->arg[1])
        ticks = cmd->arg[1];
    snd_tempo_scale_step = (((signed char)cmd->arg[2] << 16) - from) / ticks;
    snd_tempo_scale_ticks = ticks;
}

void snd_cmd_D4(SoundCommand *cmd)
{
    snd_pitch_scale = (signed char)cmd->arg[0] << 16;
    snd_pitch_scale_ticks = 0;
}

void snd_cmd_D5(SoundCommand *cmd)
{
    int ticks = 1;

    if (cmd->arg[0])
        ticks = cmd->arg[0];
    snd_pitch_scale_step = (((signed char)cmd->arg[1] << 16) - snd_pitch_scale) / ticks;
    snd_pitch_scale_ticks = ticks;
}

void snd_cmd_D6(SoundCommand *cmd)
{
    int ticks = 1;
    int from = (signed char)cmd->arg[0] << 16;

    snd_pitch_scale = from;
    if (cmd->arg[1])
        ticks = cmd->arg[1];
    snd_pitch_scale_step = (((signed char)cmd->arg[2] << 16) - from) / ticks;
    snd_pitch_scale_ticks = ticks;
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

    snd_muted_voices = cmd->arg[0];
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
    unsigned short voice;

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
    unsigned short voice;

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
    snd_reverb_balance = cmd->arg[0] & 0x7F;
    seq_song_flags |= SONG_UPDATE_REVERB;
}

/* E4 n: stores n (a byte) and sets flag 0x10 at 0x80062FF8 when it's non-zero; the reverb is updated. */
void snd_cmd_E4(SoundCommand *cmd)
{
    unsigned short n = (unsigned char)cmd->arg[0];

    snd_reverb_scale = n;
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

/* F8/F9: after snd_stream_start, the stream's voices leave the effects, and their reverb goes off (F8) or on (F9).
 * The game reaches effect_voices through a pointer here. */
void snd_cmd_F8(SoundCommand *cmd)
{
    unsigned int *voices = &effect_voices;

    snd_stream_start(cmd);
    *voices &= ~snd_stream_voices;
    effect_reverb_voices &= ~snd_stream_voices;
    snd_apply_reverb_voices();
}

void snd_cmd_F9(SoundCommand *cmd)
{
    unsigned int *voices = &effect_voices;

    snd_stream_start(cmd);
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

/* Starts the sample stream at arg 0, at volume arg 2 and pan arg 1 (mono). The header: total size, flags (bit 0
 * stereo, bit 1 loops; the top half is the pitch), the loop offset, and a spare word. The first 8 KB go to the SPU
 * at once, and the stream's voices leave the effects' noise and FM and the 0x30 effect pair. */
void snd_stream_start(SoundCommand *cmd)
{
    unsigned int flags;
    unsigned int loop;

    effect_key_off_voices |= snd_stream_voices;
    func_80038FEC(0);
    func_80038A84(0);
    SpuSetIRQCallback(0);
    snd_stream_pos = (unsigned char *)cmd->arg[0];
    snd_stream_pan = cmd->arg[1];
    snd_stream_volume = cmd->arg[2] << 7;
    snd_stream_left = *(unsigned int *)snd_stream_pos;
    if (snd_stream_left) {
        snd_stream_pos += 4;
        flags = *(unsigned int *)snd_stream_pos;
        snd_stream_flags = flags;
        snd_stream_pos += 4;
        loop = *(unsigned int *)snd_stream_pos;
        snd_stream_pos += 8;
        if (flags & 2)
            snd_stream_loop = snd_stream_pos + loop;
        else
            snd_stream_loop = 0;
        if (flags & 2)
            snd_stream_loop_size = snd_stream_left - loop;
        else
            snd_stream_loop_size = 0;
        if (flags & 1) {
            snd_stream_voices_stereo();
            func_80038FEC(snd_stream_play_stereo);
            snd_stream_voices = 0x30000;
        } else {
            snd_stream_voice_mono();
            func_80038FEC(snd_stream_play_mono);
            snd_stream_voices = 0x10000;
        }
        func_80038FB8(0);
        func_80038F64(0x77000);
        func_80038F04(snd_stream_pos, 0x2000);
        if (snd_stream_left > 0x2000) {
            snd_stream_left -= 0x2000;
            snd_stream_pos += 0x2000;
        } else {
            snd_stream_left = 0;
        }
    }
    effect_fm_voices &= ~snd_stream_voices;
    effect_noise_voices &= ~snd_stream_voices;
    snd_apply_fm_voices();
    snd_apply_noise_voices();
    snd_stop_effect_pairs(0, 1);
}

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

/* The game's way in: it fills snd_request and calls this, which queues the matching driver commands with the queue
 * locked. Returns 0, 1 when the song asked for is already playing, or -1 when its data has no AKAO header. */
int snd_submit_request(void)
{
    SoundCommand *q;
    int result = 0;
    unsigned char *p;
    unsigned short song_id;
    unsigned int size;
    unsigned int mode;

    snd_queue_locked = 1;
    switch (snd_request.id) {
    case 0x10: /* song starts: the data must begin with an AKAO header (id, size, reverb mode) */
    case 0x14:
    case 0x15:
    case 0x18:
    case 0x19:
        p = (unsigned char *)snd_request.arg[0];
        if (p[0] == 'A' && p[1] == 'K' && p[2] == 'A' && p[3] == 'O') {
            p += 4;
            song_id = *(unsigned short *)p;
            p += 2;
            size = *(unsigned short *)p;
            p += 2;
            mode = *(unsigned short *)p;
            p += 8;
            if (seq_song_id != song_id) {
                snd_set_reverb_mode(mode);
                snd_queue_next(&q);
                q->arg[0] = (int)p;
                q->arg[1] = size;
                q->arg[2] = song_id;
                q->arg[3] = snd_request.arg[1];
                q->id = snd_request.id;
            } else {
                result = 1;
            }
        } else {
            result = -1;
        }
        break;
    case 0x24: /* effects on 1-4 channel pairs, with consecutive ids */
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->id = 0x20;
        break;
    case 0x25:
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->arg[2] = snd_request.arg[1] + 1;
        q->id = 0x21;
        break;
    case 0x26:
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->arg[2] = snd_request.arg[1] + 1;
        q->arg[3] = snd_request.arg[1] + 2;
        q->id = 0x22;
        break;
    case 0x27:
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->arg[2] = snd_request.arg[1] + 1;
        q->arg[3] = snd_request.arg[1] + 2;
        q->arg[4] = snd_request.arg[1] + 3;
        q->id = 0x23;
        break;
    case 0xD8: /* tempo and pitch scale together */
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->id = 0xD0;
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->id = 0xD4;
        break;
    case 0xD9:
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->id = 0xD1;
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->id = 0xD5;
        break;
    case 0xDA:
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->arg[2] = snd_request.arg[2];
        q->id = 0xD2;
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->arg[2] = snd_request.arg[2];
        q->id = 0xD6;
        break;
    case 0x99: /* pause music and effects */
        snd_queue_next(&q);
        q->id = 0x9B;
        snd_queue_next(&q);
        q->id = 0x9D;
        break;
    case 0x98: /* resume both */
        snd_queue_next(&q);
        q->id = 0x9A;
        snd_queue_next(&q);
        q->id = 0x9C;
        break;
    default:
        snd_queue_next(&q);
        q->arg[0] = snd_request.arg[0];
        q->arg[1] = snd_request.arg[1];
        q->arg[2] = snd_request.arg[2];
        q->arg[3] = snd_request.arg[3];
        q->arg[4] = snd_request.arg[4];
        q->id = snd_request.id;
        break;
    }
    snd_queue_locked = 0;
    return result;
}

/* Runs one sound command now. Some ids are shorthands: 24-27 play 1-4 effects with consecutive ids (as 20-23), 98/99
 * resume or pause music and effects together (9A + 9C, 9B + 9D), and D8-DA run D0-D2 and D4-D6 together. The song
 * commands (10, 14, 15, 18, 19) do nothing here. */
int snd_run_command(SoundCommand *cmd)
{
    switch (cmd->id) {
    case 0x10:
    case 0x14:
    case 0x15:
    case 0x18:
    case 0x19:
        cmd->id = 0;
        break;
    case 0x24:
        cmd->id = 0x20;
        break;
    case 0x25:
        cmd->id = 0x21;
        cmd->arg[2] = cmd->arg[1] + 1;
        break;
    case 0x26:
        cmd->id = 0x22;
        cmd->arg[2] = cmd->arg[1] + 1;
        cmd->arg[3] = cmd->arg[1] + 2;
        break;
    case 0x27:
        cmd->id = 0x23;
        cmd->arg[2] = cmd->arg[1] + 1;
        cmd->arg[3] = cmd->arg[1] + 2;
        cmd->arg[4] = cmd->arg[1] + 3;
        break;
    case 0xD8:
        snd_command_handlers[0xD0](cmd);
        cmd->id = 0xD4;
        break;
    case 0xD9:
        snd_command_handlers[0xD1](cmd);
        cmd->id = 0xD5;
        break;
    case 0xDA:
        snd_command_handlers[0xD2](cmd);
        cmd->id = 0xD6;
        break;
    case 0x99:
        snd_command_handlers[0x9B](cmd);
        cmd->id = 0x9D;
        break;
    case 0x98:
        snd_command_handlers[0x9A](cmd);
        cmd->id = 0x9C;
        break;
    }
    snd_command_handlers[(unsigned char)cmd->id](cmd);
    return 0;
}

/* Runs every queued sound command, unless the queue is held. */
void snd_flush_queue(void)
{
    SoundCommand *cmd;

    if (snd_queue_locked)
        return;
    cmd = snd_queue;
    while (snd_queue_count) {
        snd_command_handlers[(unsigned char)cmd->id](cmd);
        snd_queue_count--;
        cmd++;
    }
}

/* Hands a voice's settings to libspu, each group whose update bits are set, clearing them as it goes. */
void snd_update_voice(unsigned int voice, VoiceSettings *v)
{
    if (v->update & UPDATE_PITCH) {
        func_800395C8(voice, v->pitch);
        if (!(v->update &= ~UPDATE_PITCH))
            return;
    }
    if (v->update & UPDATE_VOLUME) {
        func_800393C8(voice, v->volume[0], v->volume[1]);
        if (!(v->update &= ~UPDATE_VOLUME))
            return;
    }
    if (v->update & UPDATE_SAMPLE) {
        func_80039644(voice, v->sample);
        if (!(v->update &= ~UPDATE_SAMPLE))
            return;
    }
    if (v->update & UPDATE_LOOP) {
        func_800396C0(voice, v->loop);
        if (!(v->update &= ~UPDATE_LOOP))
            return;
    }
    if (v->update & (UPDATE_SUSTAIN_MODE | UPDATE_SUSTAIN_RATE)) {
        SpuSetVoiceSRAttr(voice, v->sustain_rate, v->sustain_mode);
        if (!(v->update &= ~(UPDATE_SUSTAIN_MODE | UPDATE_SUSTAIN_RATE)))
            return;
    }
    if (v->update & (UPDATE_ATTACK_MODE | UPDATE_ATTACK_RATE)) {
        SpuSetVoiceARAttr(voice, v->attack_rate, v->attack_mode);
        if (!(v->update &= ~(UPDATE_ATTACK_MODE | UPDATE_ATTACK_RATE)))
            return;
    }
    if (v->update & (UPDATE_RELEASE_MODE | UPDATE_RELEASE_RATE)) {
        SpuSetVoiceRRAttr(voice, v->release_rate, v->release_mode);
        if (!(v->update &= ~(UPDATE_RELEASE_MODE | UPDATE_RELEASE_RATE)))
            return;
    }
    if (v->update & (UPDATE_DECAY_RATE | UPDATE_SUSTAIN_LEVEL)) {
        SpuSetVoiceDR(voice, v->decay_rate);
        SpuSetVoiceSL(voice, v->sustain_level);
        v->update &= ~(UPDATE_DECAY_RATE | UPDATE_SUSTAIN_LEVEL);
    }
}

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

/* A music channel's per-tick slides: volume, balance (a layered channel's volume follows it), pan, the vibrato and
 * tremolo delays, the timed noise and FM flips, the three LFO depth slides (their output waits for the delays), and
 * the slide on +0x34. Changes mark the voice settings to rewrite. The noise flip goes through a pointer: only then
 * does GCC keep the flags write after it, as the game does. */
void seq_channel_slides(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int value;
    unsigned int *noise;
    unsigned short depth;
    unsigned int amount;
    short *wave;

    if (ch->volume_slide_ticks) {
        ch->volume_slide_ticks--;
        value = ch->volume + ch->volume_slide_step;
        if ((value & 0xFFE00000) != (ch->volume & 0xFFE00000))
            ch->voice.update |= UPDATE_VOLUME;
        ch->volume = value;
    }
    if (ch->balance_slide_ticks) {
        ch->balance_slide_ticks--;
        value = ch->balance + ch->balance_slide_step;
        if ((ch->flags & CHANNEL_LAYER) && (value & 0xFF00) != (ch->balance & 0xFF00))
            ch->voice.update |= UPDATE_VOLUME;
        ch->balance = value;
    }
    if (ch->pan_slide_ticks) {
        ch->pan_slide_ticks--;
        value = ch->pan + ch->pan_slide_step;
        if ((value & 0xFF00) != (ch->pan & 0xFF00))
            ch->voice.update |= UPDATE_VOLUME;
        ch->pan = value;
    }
    if (ch->vibrato_delay_count)
        ch->vibrato_delay_count--;
    if (ch->tremolo_delay_count)
        ch->tremolo_delay_count--;
    if (ch->noise_ticks) {
        if (!--ch->noise_ticks) {
            noise = &song->noise_voices;
            *noise ^= voice;
            seq_song_flags |= SONG_UPDATE_NOISE;
            snd_apply_noise_voices();
        }
    }
    if (ch->fm_ticks) {
        if (!--ch->fm_ticks) {
            song->fm_voices ^= voice;
            snd_apply_fm_voices();
        }
    }
    if (ch->vibrato_slide_ticks) {
        ch->vibrato_slide_ticks--;
        depth = ch->vibrato_depth += ch->vibrato_slide_step;
        amount = (depth & 0x7F00) >> 8;
        ch->vibrato_amount = (amount * (depth & 0x8000 ? ch->note_pitch : ((unsigned int)ch->note_pitch * 15) >> 8)) >> 7;
        if (!ch->vibrato_delay_count && ch->vibrato_step_count != 1) {
            wave = ch->vibrato_wave_pos;
            if (!wave[0] && !wave[1])
                wave += wave[2];
            value = ((unsigned short)ch->vibrato_amount * *wave) >> 16;
            if (value != ch->vibrato_offset) {
                ch->vibrato_offset = value;
                ch->voice.update |= UPDATE_PITCH;
                if (value >= 0)
                    ch->vibrato_offset = value * 2;
            }
        }
    }
    if (ch->tremolo_slide_ticks) {
        ch->tremolo_slide_ticks--;
        ch->tremolo_depth += ch->tremolo_slide_step;
        if (!ch->tremolo_delay_count && ch->tremolo_step_count != 1) {
            wave = ch->tremolo_wave_pos;
            if (!wave[0] && !wave[1])
                wave += wave[2];
            value = (int)((((short)(ch->volume >> 16) * ch->volume_scale) >> 7) * (ch->tremolo_depth >> 8) << 9) >> 16;
            value = (value * *wave) >> 15;
            if (value != ch->tremolo_offset) {
                ch->tremolo_offset = value;
                ch->voice.update |= UPDATE_VOLUME;
            }
        }
    }
    if (ch->pan_lfo_slide_ticks) {
        ch->pan_lfo_slide_ticks--;
        ch->pan_lfo_depth += ch->pan_lfo_slide_step;
        if (ch->pan_lfo_step_count != 1) {
            wave = ch->pan_lfo_wave_pos;
            if (!wave[0] && !wave[1])
                wave += wave[2];
            value = ((ch->pan_lfo_depth >> 8) * *wave) >> 15;
            if (value != ch->pan_lfo_offset) {
                ch->pan_lfo_offset = value;
                ch->voice.update |= UPDATE_VOLUME;
            }
        }
    }
    if (ch->bend_count) {
        ch->bend_count--;
        value = ch->bend_offset + ch->bend_step;
        if ((value & 0xFFFF0000) != (ch->bend_offset & 0xFFFF0000))
            ch->voice.update |= UPDATE_PITCH;
        ch->bend_offset = value;
    }
}

/* An effect channel's per-tick slides: volume, the timed noise and FM flips (on the effects' own masks), the three
 * LFO depth slides with their output recomputed, and the slide on +0x34. Changes mark the voice settings to rewrite. */
void seq_effect_slides(SeqChannel *ch, unsigned int voice)
{
    int value;
    unsigned short depth;
    unsigned int amount;
    short *wave;

    if (ch->volume_slide_ticks) {
        ch->volume_slide_ticks--;
        value = ch->volume + ch->volume_slide_step;
        if ((value & 0xFFE00000) != (ch->volume & 0xFFE00000))
            ch->voice.update |= UPDATE_VOLUME;
        ch->volume = value;
    }
    if (ch->noise_ticks) {
        if (!--ch->noise_ticks) {
            effect_noise_voices ^= voice;
            seq_song_flags |= SONG_UPDATE_NOISE;
            snd_apply_noise_voices();
        }
    }
    if (ch->fm_ticks) {
        if (!--ch->fm_ticks) {
            effect_fm_voices ^= voice;
            snd_apply_fm_voices();
        }
    }
    if (ch->vibrato_slide_ticks) {
        ch->vibrato_slide_ticks--;
        depth = ch->vibrato_depth += ch->vibrato_slide_step;
        amount = (depth & 0x7F00) >> 8;
        ch->vibrato_amount = (amount * (depth & 0x8000 ? ch->note_pitch : ((unsigned int)ch->note_pitch * 15) >> 8)) >> 7;
        if (ch->vibrato_step_count != 1) {
            wave = ch->vibrato_wave_pos;
            if (!wave[0] && !wave[1])
                wave += wave[2];
            value = ((unsigned short)ch->vibrato_amount * *wave) >> 16;
            if (value != ch->vibrato_offset) {
                ch->vibrato_offset = value;
                ch->voice.update |= UPDATE_PITCH;
                if (value >= 0)
                    ch->vibrato_offset = value * 2;
            }
        }
    }
    if (ch->tremolo_slide_ticks) {
        ch->tremolo_slide_ticks--;
        ch->tremolo_depth += ch->tremolo_slide_step;
        if (ch->tremolo_step_count != 1) {
            wave = ch->tremolo_wave_pos;
            if (!wave[0] && !wave[1])
                wave += wave[2];
            value = (int)((((short)(ch->volume >> 16) * ch->volume_scale) >> 7) * (ch->tremolo_depth >> 8) << 9) >> 16;
            value = (value * *wave) >> 15;
            if (value != ch->tremolo_offset) {
                ch->tremolo_offset = value;
                ch->voice.update |= UPDATE_VOLUME;
            }
        }
    }
    if (ch->pan_lfo_slide_ticks) {
        ch->pan_lfo_slide_ticks--;
        ch->pan_lfo_depth += ch->pan_lfo_slide_step;
        if (ch->pan_lfo_step_count != 1) {
            wave = ch->pan_lfo_wave_pos;
            if (!wave[0] && !wave[1])
                wave += wave[2];
            value = ((ch->pan_lfo_depth >> 8) * *wave) >> 15;
            if (value != ch->pan_lfo_offset) {
                ch->pan_lfo_offset = value;
                ch->voice.update |= UPDATE_VOLUME;
            }
        }
    }
    if (ch->bend_count) {
        ch->bend_count--;
        value = ch->bend_offset + ch->bend_step;
        if ((value & 0xFFFF0000) != (ch->bend_offset & 0xFFFF0000))
            ch->voice.update |= UPDATE_PITCH;
        ch->bend_offset = value;
    }
}

/* A music channel's per-tick voice update. Each LFO that's on (and past its delay) steps through its waveform. Then,
 * if the volume is to be rewritten: volume x A3 scale (or the previous channel's pitch, D6), plus tremolo, x the
 * master volume, x the cross-fade, split by pan (+ pan LFO) through the pan tables by output mode: 1 stereo, 4 the
 * same with one side of each voice inverted, else mono. The pitch register is the note's (or the previous
 * channel's, D4) plus vibrato and the +0x34 offset, scaled by D4-D6's global pitch scale. */
void seq_channel_lfo(SeqChannel *ch, unsigned int voice, unsigned short index)
{
    int volume = ((ch->volume >> 16) * ch->volume_scale) >> 7;
    int value;
    int pitch;

    if (ch->flags & CHANNEL_VIBRATO && !ch->vibrato_delay_count && !--ch->vibrato_step_count) {
        ch->vibrato_step_count = ch->vibrato_rate;
        if (!ch->vibrato_wave_pos[0] && !ch->vibrato_wave_pos[1])
            ch->vibrato_wave_pos += ch->vibrato_wave_pos[2];
        pitch = *ch->vibrato_wave_pos++;
        value = ((unsigned short)ch->vibrato_amount * pitch) >> 16;
        if (value != ch->vibrato_offset) {
            ch->vibrato_offset = value;
            ch->voice.update |= UPDATE_PITCH;
            if (value >= 0)
                ch->vibrato_offset = value * 2;
        }
    }
    if (ch->flags & CHANNEL_TREMOLO && !ch->tremolo_delay_count && !--ch->tremolo_step_count) {
        ch->tremolo_step_count = ch->tremolo_rate;
        if (!ch->tremolo_wave_pos[0] && !ch->tremolo_wave_pos[1])
            ch->tremolo_wave_pos += ch->tremolo_wave_pos[2];
        value = (int)(volume * (ch->tremolo_depth >> 8) << 9) >> 16;
        value = (value * *ch->tremolo_wave_pos++) >> 15;
        if (value != ch->tremolo_offset) {
            ch->tremolo_offset = value;
            ch->voice.update |= UPDATE_VOLUME;
        }
    }
    if (ch->flags & CHANNEL_PAN_LFO && !--ch->pan_lfo_step_count) {
        ch->pan_lfo_step_count = ch->pan_lfo_rate;
        if (!ch->pan_lfo_wave_pos[0] && !ch->pan_lfo_wave_pos[1])
            ch->pan_lfo_wave_pos += ch->pan_lfo_wave_pos[2];
        value = ((ch->pan_lfo_depth >> 8) * *ch->pan_lfo_wave_pos++) >> 15;
        if (value != ch->pan_lfo_offset) {
            ch->pan_lfo_offset = value;
            ch->voice.update |= UPDATE_VOLUME;
        }
    }
    if (ch->flags & CHANNEL_VOLUME_FOLLOW) {
        volume = ((short)(ch[-1].voice.pitch * 2) * ch->volume_scale) >> 7;
        ch->voice.update |= UPDATE_VOLUME;
    }
    if (ch->voice.update & UPDATE_VOLUME) {
        volume += ch->tremolo_offset;
        volume = (volume * (snd_master_volume_top & 0x7F)) >> 7;
        if (snd_channel_fades[0].ticks)
            volume = (volume * (short)(snd_channel_fades[index].volume >> 16)) >> 7;
        value = ((ch->pan >> 8) + ch->pan_lfo_offset) & 0xFF;
        switch (seq_song.unk_00) {
        case 1:
            ch->voice.volume[0] = (volume * snd_pan_left[value]) >> 15;
            ch->voice.volume[1] = (volume * snd_pan_right[value]) >> 15;
            break;
        case 4:
            ch->voice.volume[0] = (volume * snd_pan_left[value]) >> 15;
            ch->voice.volume[1] = (volume * snd_pan_right[value]) >> 15;
            if (voice & 0xAAAAAA)
                ch->voice.volume[1] = ~ch->voice.volume[1];
            else
                ch->voice.volume[0] = ~ch->voice.volume[0];
            break;
        default:
            ch->voice.volume[0] = ch->voice.volume[1] = (unsigned int)(volume * snd_pan_left[64]) >> 15;
            break;
        }
    }
    if (ch->flags & CHANNEL_PITCH_FOLLOW) {
        value = (unsigned char)(snd_pitch_scale >> 16);
        pitch = ch[-1].voice.pitch + ch->vibrato_offset + (ch->bend_offset >> 16);
        if (value) {
            if (value < 0x80)
                pitch += (pitch * value) >> 7;
            else
                pitch = (pitch * value) >> 8;
        }
        ch->voice.pitch = pitch & 0x3FFF;
        ch->voice.update |= UPDATE_PITCH;
    } else if (ch->voice.update & UPDATE_PITCH) {
        value = (unsigned char)(snd_pitch_scale >> 16);
        pitch = ch->vibrato_offset + ch->note_pitch + (ch->bend_offset >> 16);
        if (value) {
            if (value < 0x80)
                pitch += (pitch * value) >> 7;
            else
                pitch = (pitch * value) >> 8;
        }
        ch->voice.pitch = pitch & 0x3FFF;
    }
}

/* An effect channel's per-tick voice update, as seq_channel_lfo: no LFO delays, master volume or cross-fade, but the
 * effect's balance scales its volume and its pitch shift scales its pitch, except for the kind 2 effect (centred). */
void seq_effect_lfo(SeqChannel *ch, unsigned int voice)
{
    int volume = ((ch->volume >> 16) * ch->volume_scale) >> 7;
    int value;
    int pitch;

    if (ch->flags & CHANNEL_VIBRATO && !--ch->vibrato_step_count) {
        ch->vibrato_step_count = ch->vibrato_rate;
        if (!ch->vibrato_wave_pos[0] && !ch->vibrato_wave_pos[1])
            ch->vibrato_wave_pos += ch->vibrato_wave_pos[2];
        pitch = *ch->vibrato_wave_pos++;
        value = ((unsigned short)ch->vibrato_amount * pitch) >> 16;
        if (value != ch->vibrato_offset) {
            ch->vibrato_offset = value;
            ch->voice.update |= UPDATE_PITCH;
            if (value >= 0)
                ch->vibrato_offset = value * 2;
        }
    }
    if (ch->flags & CHANNEL_TREMOLO && !--ch->tremolo_step_count) {
        ch->tremolo_step_count = ch->tremolo_rate;
        if (!ch->tremolo_wave_pos[0] && !ch->tremolo_wave_pos[1])
            ch->tremolo_wave_pos += ch->tremolo_wave_pos[2];
        value = (int)(volume * (ch->tremolo_depth >> 8) << 9) >> 16;
        pitch = *ch->tremolo_wave_pos++;
        value = (value * pitch) >> 15;
        if (value != ch->tremolo_offset) {
            ch->tremolo_offset = value;
            ch->voice.update |= UPDATE_VOLUME;
        }
    }
    if (ch->flags & CHANNEL_PAN_LFO && !--ch->pan_lfo_step_count) {
        ch->pan_lfo_step_count = ch->pan_lfo_rate;
        if (!ch->pan_lfo_wave_pos[0] && !ch->pan_lfo_wave_pos[1])
            ch->pan_lfo_wave_pos += ch->pan_lfo_wave_pos[2];
        pitch = *ch->pan_lfo_wave_pos++;
        value = ((ch->pan_lfo_depth >> 8) * pitch) >> 15;
        if (value != ch->pan_lfo_offset) {
            ch->pan_lfo_offset = value;
            ch->voice.update |= UPDATE_VOLUME;
        }
    }
    if (ch->flags & CHANNEL_VOLUME_FOLLOW) {
        volume = ((short)(ch[-1].voice.pitch * 2) * ch->volume_scale) >> 7;
        ch->voice.update |= UPDATE_VOLUME;
    }
    if (ch->voice.update & UPDATE_VOLUME) {
        volume += ch->tremolo_offset;
        value = 0x40;
        if (ch->is_effect != 2) {
            volume = (volume * (ch->balance >> 8)) >> 7;
            value = ((ch->pan >> 8) + ch->pan_lfo_offset) & 0xFF;
        }
        switch (seq_song.unk_00) {
        case 1:
            ch->voice.volume[0] = (volume * snd_pan_left[value]) >> 15;
            ch->voice.volume[1] = (volume * snd_pan_right[value]) >> 15;
            break;
        case 4:
            ch->voice.volume[0] = (volume * snd_pan_left[value]) >> 15;
            ch->voice.volume[1] = (volume * snd_pan_right[value]) >> 15;
            if (voice & 0xAAAAAA)
                ch->voice.volume[1] = ~ch->voice.volume[1];
            else
                ch->voice.volume[0] = ~ch->voice.volume[0];
            break;
        default:
            ch->voice.volume[0] = ch->voice.volume[1] = (unsigned int)(volume * snd_pan_left[64]) >> 15;
            break;
        }
    }
    if (ch->flags & CHANNEL_PITCH_FOLLOW) {
        pitch = ch[-1].voice.pitch + ch->vibrato_offset + (ch->bend_offset >> 16);
        if (ch->is_effect != 2 && (value = (unsigned char)(ch->pitch_shift >> 8))) {
            if (value < 0x80)
                pitch += (pitch * value) >> 7;
            else
                pitch = (pitch * value) >> 8;
        }
        ch->voice.pitch = pitch & 0x3FFF;
        ch->voice.update |= UPDATE_PITCH;
    } else if (ch->voice.update & UPDATE_PITCH) {
        pitch = ch->vibrato_offset + ch->note_pitch + (ch->bend_offset >> 16);
        if (ch->is_effect != 2 && (value = (unsigned char)(ch->pitch_shift >> 8))) {
            if (value < 0x80)
                pitch += (pitch * value) >> 7;
            else
                pitch = (pitch * value) >> 8;
        }
        ch->voice.pitch = pitch & 0x3FFF;
    }
}

/* A layered channel's voice update: the main voice keeps (127 - balance) / 256 of its volume and the layer voice
 * gets balance / 256, the layer plays the main voice's pitch, and both are written (the layer if it's in voices). */
void seq_layer_voice_update(SeqChannel *ch, unsigned int voices, unsigned int voice)
{
    SeqChannel *layer;
    unsigned short share = 127 - (ch->balance >> 8);
    short vol;

    vol = ch->voice.volume[0];
    ch->voice.volume[0] = (vol * share) >> 8;
    layer = &seq_channels[ch->layer_voice];
    layer->voice.volume[0] = (vol * ch->balance) >> 16;
    vol = ch->voice.volume[1];
    ch->voice.volume[1] = (vol * share) >> 8;
    layer->voice.volume[1] = (vol * ch->balance) >> 16;
    layer->voice.pitch = ch->voice.pitch;
    layer->voice.update |= ch->voice.update;
    snd_update_voice(ch->voice.voice, &ch->voice);
    if (voices & (1 << voice))
        snd_update_voice(voice, &layer->voice);
}

/* The per-tick output pass. A changed reverb depth goes to the SPU (scaled by E4's value, split left/right by E0's)
 * and so does a changed noise clock. Then for both songs and the effects, every voice due an update runs its LFO
 * update and writes what changed: a layered channel through seq_layer_voice_update, an overlap channel on whichever
 * voice its turn is on (flag 0x400 flips at each key-on). Muted voices (90) get no volume. Last, the voices due a
 * key-on are keyed on together. */
void seq_update_voices(void)
{
    unsigned int key_on = 0;
    int depth;
    unsigned int voices;
    unsigned int bit;
    unsigned int free;
    unsigned int index;
    SeqChannel *ch;

    if (seq_song_flags & SONG_UPDATE_REVERB) {
        depth = seq_song.reverb_depth >> 16;
        snd_reverb_attr.mask = 6;
        if (snd_reverb_scale < 0x80)
            depth += (depth * snd_reverb_scale) >> 7;
        else
            depth = (depth * snd_reverb_scale) >> 8;
        if (snd_reverb_balance < 0x40) {
            snd_reverb_attr.depth[0] = depth;
            snd_reverb_attr.depth[1] = depth - ((depth * (snd_reverb_balance ^ 0x3F)) >> 6);
        } else {
            snd_reverb_attr.depth[1] = depth;
            snd_reverb_attr.depth[0] = depth - ((depth * (snd_reverb_balance & 0x3F)) >> 6);
        }
        SpuSetReverbDepth(&snd_reverb_attr);
        seq_song_flags ^= SONG_UPDATE_REVERB;
    }
    if (seq_song_flags & SONG_UPDATE_NOISE) {
        SpuSetNoiseClock(effect_voices ? effect_noise_clock : seq_song.noise_clock);
        seq_song_flags ^= SONG_UPDATE_NOISE;
    }
    voices = seq_song2.active_voices;
    if (voices) {
        bit = 1;
        index = 0;
        ch = &seq_channels[24];
        free = ~(effect_voices | snd_stream_voices) & D_80062F68;
        voices &= free & seq_song2.update_voices;
        key_on = free & seq_song2.key_on_voices;
        do {
            if (voices & bit) {
                seq_channel_lfo(ch, bit, index);
                if (ch->voice.update) {
                    if (ch->flags & CHANNEL_LAYER) {
                        seq_layer_voice_update(ch, free, ch->layer_voice - 24);
                    } else if (ch->flags & CHANNEL_OVERLAP) {
                        if (seq_song2.key_on_voices & bit) {
                            ch->flags ^= 0x400;
                            ch->voice.update |= UPDATE_ALL;
                        }
                        if (ch->flags & 0x400) {
                            if (free & (1 << ch->overlap_voice)) {
                                snd_update_voice(ch->overlap_voice, &ch->voice);
                                if (key_on & bit) {
                                    key_on |= 1 << ch->overlap_voice;
                                    key_on &= ~bit;
                                }
                            }
                        } else {
                            snd_update_voice(ch->voice.voice, &ch->voice);
                        }
                    } else {
                        snd_update_voice(ch->voice.voice, &ch->voice);
                    }
                }
                voices ^= bit;
            }
            bit <<= 1;
            ch++;
            index++;
        } while (voices);
        seq_song2.key_on_voices = 0;
    }
    voices = seq_song_active_voices;
    if (voices) {
        bit = 1;
        index = 0;
        ch = seq_channels;
        free = ~(D_80062F68 | effect_voices | snd_stream_voices);
        voices &= free & seq_song_update_voices;
        key_on |= free & seq_song_key_on_voices;
        do {
            if (voices & bit) {
                seq_channel_lfo(ch, bit, index);
                if (ch->voice.update) {
                    if (snd_muted_voices & bit) {
                        ch->voice.volume[1] = 0;
                        ch->voice.volume[0] = 0;
                    }
                    if (ch->flags & CHANNEL_LAYER) {
                        seq_layer_voice_update(ch, free, ch->layer_voice);
                    } else if (ch->flags & CHANNEL_OVERLAP) {
                        if (seq_song_key_on_voices & bit) {
                            ch->flags ^= 0x400;
                            ch->voice.update |= UPDATE_ALL;
                        }
                        if (ch->flags & 0x400 && free & (1 << ch->overlap_voice)) {
                            snd_update_voice(ch->overlap_voice, &ch->voice);
                            if (key_on & bit) {
                                key_on |= 1 << ch->overlap_voice;
                                key_on &= ~bit;
                            }
                        } else {
                            snd_update_voice(ch->voice.voice, &ch->voice);
                        }
                    } else {
                        snd_update_voice(ch->voice.voice, &ch->voice);
                    }
                }
                voices ^= bit;
            }
            bit <<= 1;
            ch++;
            index++;
        } while (voices);
        seq_song_key_on_voices = 0;
    }
    voices = effect_voices;
    if (voices) {
        bit = 0x10000;
        ch = &seq_channels[48];
        key_on |= effect_key_on_voices;
        voices &= effect_update_voices;
        do {
            if (voices & bit) {
                seq_effect_lfo(ch, bit);
                voices ^= bit;
                if (ch->voice.update)
                    snd_update_voice(ch->voice.voice, &ch->voice);
            }
            bit <<= 1;
            ch++;
        } while (voices);
        effect_key_on_voices = 0;
    }
    if (key_on)
        func_80038C6C(1, key_on);
}

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

/* Every 4th tick: the CD volume, master volume (a fade to 0 stops the songs), tempo and pitch scale slides; each
 * effect's balance (a fade to 0 ends it), pan and pitch shift slides; and the cross-fade, where each faded-out
 * channel of the old song ends and the new song's channel comes back in. */
void snd_slow_slides(void)
{
    unsigned int voices;
    unsigned int bit;
    SeqChannel *ch;
    SeqChannel *main;
    ChannelFade *fade;
    int value;
    unsigned char **pos;
    unsigned int *active;

    if (++snd_slow_slide_tick & 3)
        return;
    if (snd_cd_volume_slide_ticks) {
        snd_cd_volume_slide_ticks--;
        snd_cd_volume += snd_cd_volume_slide_step;
        snd_apply_cd_volume();
    }
    if (!(D_80062FF8 & 1)) {
        if (snd_master_volume_ticks) {
            value = snd_master_volume + snd_master_volume_step;
            if (!--snd_master_volume_ticks && !(value & 0x7F0000) && snd_master_volume_step < 0)
                snd_stop_songs();
            else if ((value & 0x7F0000) != (snd_master_volume & 0x7F0000))
                snd_refresh_song_volumes();
            snd_master_volume = value;
        }
        if (snd_tempo_scale_ticks) {
            snd_tempo_scale_ticks--;
            snd_tempo_scale += snd_tempo_scale_step;
        }
        if (snd_pitch_scale_ticks) {
            snd_pitch_scale_ticks--;
            value = snd_pitch_scale + snd_pitch_scale_step;
            if ((value & 0xFF0000) != (snd_pitch_scale & 0xFF0000)) {
                ch = seq_channels;
                for (bit = 24; bit; bit--) {
                    ch->voice.update |= UPDATE_PITCH;
                    ch++;
                }
            }
            snd_pitch_scale = value;
        }
    }
    voices = effect_voices;
    if (voices) {
        ch = &seq_channels[48];
        bit = 0x10000;
        do {
            if (voices & bit) {
                if (ch->balance_slide_ticks) {
                    ch->balance_slide_ticks += (unsigned short)-1;
                    value = ch->balance + ch->balance_slide_step;
                    if (!ch->balance_slide_ticks && !(value & 0xFF00) && ch->balance_slide_step < 0) {
                        effect_key_off_voices |= bit;
                        effect_key_on_voices &= ~bit;
                        effect_update_voices &= ~bit;
                        pos = &ch->pos;
                        *pos = D_80049C40;
                    } else if ((value & 0xFF00) != (ch->balance & 0xFF00)) {
                        ch->voice.update |= UPDATE_VOLUME;
                    }
                    ch->balance = value;
                }
                if (ch->pan_slide_ticks) {
                    ch->pan_slide_ticks--;
                    value = ch->pan + ch->pan_slide_step;
                    if ((value & 0xFF00) != (ch->pan & 0xFF00))
                        ch->voice.update |= UPDATE_VOLUME;
                    ch->pan = value;
                }
                if (ch->pitch_shift_slide_ticks) {
                    ch->pitch_shift_slide_ticks--;
                    value = ch->pitch_shift + ch->pitch_shift_step;
                    if ((value & 0xFF00) != (ch->pitch_shift & 0xFF00))
                        ch->voice.update |= UPDATE_PITCH;
                    ch->pitch_shift = value;
                }
                voices ^= bit;
            }
            bit <<= 1;
            ch++;
        } while (voices);
    }
    if (!snd_channel_fades[0].ticks)
        return;
    ch = &seq_channels[24];
    main = seq_channels;
    fade = snd_channel_fades;
    bit = 1;
    do {
        /* Set every pass: the game keeps this address in a register and reaches both songs' masks from it. */
        active = &seq_songs[1].active_voices;
        if (!fade->ticks)
            return;
        fade->ticks--;
        fade->volume += fade->step;
        ch->voice.update |= UPDATE_VOLUME;
        main->voice.update |= UPDATE_VOLUME;
        if (!fade->ticks && D_80062F68 & bit) {
            D_80062F68 ^= bit;
            if (*active & bit) {
                seq_cmd_end(ch, &seq_songs[1], bit);
                seq_songs[1].key_on_voices &= ~bit;
                seq_songs[1].update_voices &= ~bit;
                seq_songs[1].key_off_voices |= bit;
            }
            if (seq_song_active_voices & bit) {
                main->voice.update |= UPDATE_ALL & ~UPDATE_VOLUME;
                seq_songs[0].key_on_voices |= seq_songs[0].update_voices & bit;
            }
            fade->step = 0x7F8000 / snd_fade_ticks;
            fade->ticks = snd_fade_ticks;
        }
        bit <<= 1;
        ch++;
        main++;
        fade++;
    } while (bit & 0xFFFFFF);
}

/* The sequencer tick (timer interrupt): sends last tick's changes to the SPU, then for each song and the effects
 * adds the tempo (scaled by D0) to the phase and, on each whole step, counts down every channel's gate: at 0 the
 * channel reads its next notes, at the end of the note's sounding part it keys off. Then the tempo, reverb and
 * meter counters advance, an F3 pause runs, the queued commands run, and every 4th tick the global slides. */
void seq_tick(void)
{
    unsigned int voices;
    unsigned int bit;
    unsigned int tempo;
    unsigned int value;
    SeqChannel *ch;
    unsigned short *gate;

    seq_update_voices();
    voices = seq_song_active_voices;
    if (voices) {
        tempo = seq_song_tempo >> 16;
        /* One variable for the scale and then the phase: the game keeps them in the same register. */
        value = (unsigned char)(snd_tempo_scale >> 16);
        if (value) {
            if (value < 0x80)
                tempo += (tempo * value) >> 7;
            else
                tempo = (tempo * value) >> 8;
        }
        value = tempo + seq_song_tempo_phase;
        seq_song_tempo_phase = value;
        if (value & 0xFFFF0000 || D_80062FF8 & 4) {
            bit = 1;
            ch = seq_channels;
            seq_song_tempo_phase = value & 0xFFFF;
            D_80062F04 = 0;
            do {
                if (voices & bit) {
                    /* Through a plain pointer: the game never moves the mask loads above this store. */
                    gate = &ch->gate;
                    *gate += 0xFEFF;
                    if (!(*gate & 0xFF)) {
                        seq_step_channel(ch, &seq_song, bit);
                    } else if (!(*gate & 0xFF00)) {
                        *gate |= 0x100;
                        seq_song_key_off_voices |= bit;
                        seq_song_update_voices &= ~bit;
                    }
                    seq_channel_slides(ch, &seq_song, bit);
                    voices ^= bit;
                }
                ch++;
                bit <<= 1;
            } while (voices);
            if (seq_song_tempo_slide_ticks) {
                seq_song_tempo_slide_ticks--;
                seq_song_tempo += seq_song_tempo_slide_step;
            }
            if (seq_song_reverb_slide_ticks) {
                seq_song_reverb_slide_ticks--;
                seq_song_reverb_depth += seq_song_reverb_slide_step;
                seq_song_flags |= SONG_UPDATE_REVERB;
            }
            if (seq_song_beat_ticks && ++seq_song_beat_tick == seq_song_beat_ticks) {
                seq_song_beat_tick = 0;
                if (++seq_song_bar_beat == seq_song_beats_per_bar) {
                    seq_song_bar_beat = 0;
                    seq_song_bar_number++;
                }
            }
        }
    }
    voices = seq_song2.active_voices;
    if (voices) {
        tempo = (unsigned int)seq_song2.tempo >> 16;
        value = (unsigned char)(snd_tempo_scale >> 16);
        if (value) {
            if (value < 0x80)
                tempo += (tempo * value) >> 7;
            else
                tempo = (tempo * value) >> 8;
        }
        value = tempo + seq_song2.tempo_phase;
        seq_song2.tempo_phase = value;
        if (value & 0xFFFF0000 || D_80062FF8 & 4) {
            seq_song2.tempo_phase = value & 0xFFFF;
            D_80062F04 = 1;
            bit = 1;
            ch = &seq_channels[24];
            do {
                if (voices & bit) {
                    gate = &ch->gate;
                    *gate += 0xFEFF;
                    if (!(*gate & 0xFF)) {
                        seq_step_channel(ch, &seq_song2, bit);
                    } else if (!(*gate & 0xFF00)) {
                        *gate |= 0x100;
                        seq_song2.key_off_voices |= bit;
                        seq_song2.update_voices &= ~bit;
                    }
                    seq_channel_slides(ch, &seq_song2, bit);
                    voices ^= bit;
                }
                ch++;
                bit <<= 1;
            } while (voices);
            if (seq_song2.tempo_slide_ticks) {
                seq_song2.tempo_slide_ticks--;
                seq_song2.tempo += seq_song2.tempo_slide_step;
            }
        }
    }
    voices = effect_voices;
    if (voices) {
        value = (effect_tempo >> 16) + effect_tempo_phase;
        effect_tempo_phase = value;
        if (value & 0xFFFF0000 || D_80062FF8 & 4) {
            effect_tempo_phase = value & 0xFFFF;
            bit = 0x10000;
            ch = &seq_channels[48];
            do {
                if (voices & bit) {
                    if (!(D_80062FF8 & 2) || ch->is_effect == 2) {
                        ch->unk_50++;
                        gate = &ch->gate;
                        *gate += 0xFEFF;
                        if (!(*gate & 0xFF)) {
                            seq_step_channel(ch, &seq_song, bit);
                        } else if (!(*gate & 0xFF00)) {
                            *gate |= 0x100;
                            effect_key_off_voices |= bit;
                            effect_update_voices &= ~bit;
                        }
                        seq_effect_slides(ch, bit);
                    }
                    voices ^= bit;
                }
                ch++;
                bit <<= 1;
            } while (voices);
        }
    }
    if (seq_song_pause_pending) {
        snd_cmd_9B(&snd_request);
        seq_song_pause_pending = 0;
    }
    snd_flush_queue();
    snd_slow_slides();
    snd_key_off_voices();
}

/* Runs a channel's next event: the commands up to the next note, tie or rest (or A0), then the note itself. Its
 * length goes in the gate, cut short by 2 ticks before a note or rest unless it slurs or plays full length (a tie
 * keeps it whole). A note sets the key, instrument (drums), pitch with detune, key-on and the LFO restarts. */
void seq_step_channel(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    unsigned char note;
    unsigned int cmd;
    unsigned int pitch;
    unsigned short length;
    unsigned char *drum;
    unsigned short octave;
    short bend;
    int layer;
    unsigned short depth;
    unsigned int amount;

    do {
        note = *ch->pos++;
        cmd = note;
        if (cmd >= 0xA0)
            seq_cmd_handlers[cmd - 0xA0](ch, song, voice);
    } while (cmd >= 0xA0 && cmd != 0xA0);
    if (cmd == 0xA0)
        return;
    /* The next note, held in pitch's register as in the game. */
    pitch = seq_look_ahead(ch);
    if (ch->fixed_length)
        ch->gate = (ch->fixed_length << 8) + ch->fixed_length;
    if (ch->gate & 0xFF) {
        if (pitch >= 0x8F || (pitch < 0x84 && !(ch->note_mode & 5)))
            ch->gate -= 0x200;
    } else {
        length = seq_note_lengths[(unsigned char)(cmd % 11)];
        if (!(pitch >= 0x84 && pitch < 0x8F) && !(ch->note_mode & 5))
            length -= 0x200;
        ch->gate = length;
    }
    ch->last_length = ch->gate & 0xFF;
    if (note >= 0x8F) {
        ch->glide_ticks = 0;
        ch->vibrato_offset = 0;
        ch->tremolo_offset = 0;
        ch->note_mode &= ~2;
        return;
    }
    if (note < 0x84) {
        /* The byte is note * 11 + length: 12 notes of 11 lengths, then ties (0x84) and rests (0x8F). */
        note = note / 11;
        if (ch->flags & CHANNEL_DRUMS) {
            if (!ch->is_effect)
                song->key_on_voices |= voice;
            else
                effect_key_on_voices |= voice;
            /* Drums: each note picks an instrument, key, volume and pan from the drum map. */
            drum = ch->drum_map;
            drum += (note % 12) * 5;
            if (drum[0] != ch->instrument) {
                ch->instrument = drum[0];
                ch->voice.sample = instrument_table[drum[0]].sample;
                ch->voice.loop = instrument_table[drum[0]].loop;
                ch->voice.attack_rate = instrument_table[drum[0]].adsr[0];
                ch->voice.decay_rate = instrument_table[drum[0]].adsr[1];
                ch->voice.sustain_level = instrument_table[drum[0]].adsr[2];
                ch->voice.sustain_rate = instrument_table[drum[0]].adsr[3];
                ch->voice.attack_mode = instrument_table[drum[0]].adsr[5];
                ch->voice.sustain_mode = instrument_table[drum[0]].adsr[6];
                if (!(ch->flags & CHANNEL_OVERLAP)) {
                    ch->voice.release_rate = instrument_table[drum[0]].adsr[4];
                    ch->voice.release_mode = instrument_table[drum[0]].adsr[7];
                    ch->voice.update |= UPDATE_SAMPLE | UPDATE_ENVELOPE | UPDATE_LOOP;
                } else {
                    ch->voice.update |= UPDATE_SAMPLE | UPDATE_LOOP | (UPDATE_ENVELOPE & ~(UPDATE_RELEASE_MODE | UPDATE_RELEASE_RATE));
                }
            }
            octave = drum[1] / 12;
            pitch = instrument_table[drum[0]].pitch[drum[1] % 12];
            if (octave >= 7)
                pitch <<= octave - 6;
            else if (octave < 6)
                pitch >>= 6 - octave;
            ch->volume = (drum[2] + (drum[3] << 8)) << 16;
            ch->pan = drum[4] << 8;
        } else {
            note += ch->octave * 12;
            /* DA: the note starts from the last one and bends to its own key. */
            if (ch->glide_ticks && ch->last_key) {
                ch->bend_ticks = ch->glide_ticks;
                ch->bend_semitones = note + ch->transpose - ch->last_key - ch->last_transpose;
                ch->key = ch->last_key - (ch->transpose - ch->last_transpose);
                note = (unsigned char)ch->last_key + (unsigned char)ch->last_transpose;
            } else {
                ch->key = note;
                note += (unsigned char)ch->transpose;
            }
            octave = (unsigned char)(note / 12);
            note = note % 12;
            if (!(ch->note_mode & 2)) {
                if (!ch->is_effect) {
                    song->key_on_voices |= voice;
                    if (ch->flags & CHANNEL_LAYER) {
                        layer = (unsigned short)ch->layer_voice;
                        if (ch->layer_voice >= 24)
                            layer -= 24;
                        song->key_on_voices |= 1 << layer;
                    }
                } else {
                    effect_key_on_voices |= voice;
                }
                ch->bend_count = 0;
            }
            pitch = instrument_table[ch->instrument].pitch[note];
            if (octave >= 7)
                pitch <<= octave - 6;
            else if (octave < 6)
                pitch >>= 6 - octave;
        }
        if (!ch->is_effect)
            song->update_voices |= voice;
        else
            effect_update_voices |= voice;
        ch->voice.update |= UPDATE_VOLUME | UPDATE_PITCH;
        if (ch->detune) {
            if (ch->detune > 0)
                pitch += (pitch * ch->detune) >> 7;
            else
                pitch += (pitch * ch->detune) >> 8;
            pitch &= 0xFFFF;
        }
        ch->note_pitch = pitch;
        if (ch->flags & CHANNEL_VIBRATO) {
            depth = ch->vibrato_depth;
            amount = (depth & 0x7F00) >> 8;
            ch->vibrato_amount = (depth & 0x8000 ? amount * pitch : amount * ((pitch * 15) >> 8)) >> 7;
            ch->vibrato_wave_pos = lfo_waveforms[ch->vibrato_wave];
            ch->vibrato_delay_count = ch->vibrato_delay;
            ch->vibrato_step_count = 1;
        }
        if (ch->flags & CHANNEL_TREMOLO) {
            ch->tremolo_wave_pos = lfo_waveforms[ch->tremolo_wave];
            ch->tremolo_delay_count = ch->tremolo_delay;
            ch->tremolo_step_count = 1;
        }
        if (ch->flags & CHANNEL_PAN_LFO) {
            ch->pan_lfo_wave_pos = lfo_waveforms[ch->pan_lfo_wave];
            ch->pan_lfo_step_count = 1;
        }
        ch->vibrato_offset = 0;
        ch->tremolo_offset = 0;
        ch->bend_offset = 0;
    }
    ch->note_mode = (ch->note_mode & ~2) | ((ch->note_mode & 1) << 1);
    /* An A4 bend (or a glide) slides bend_offset to the new key's pitch over bend_ticks. */
    bend = ch->bend_semitones;
    if (bend) {
        ch->key += ch->bend_semitones;
        note = (unsigned char)ch->key + (unsigned char)ch->transpose;
        if (!ch->is_effect) {
            pitch = instrument_table[ch->instrument].pitch[note % 12];
            if (ch->detune) {
                if (ch->detune > 0)
                    pitch += (pitch * ch->detune) >> 7;
                else
                    pitch += (pitch * ch->detune) >> 8;
                pitch &= 0xFFFF;
            }
            pitch <<= 16;
        } else {
            pitch = instrument_table[ch->instrument].pitch[note % 12] << 16;
        }
        note /= 12;
        if (note >= 7)
            pitch <<= note - 6;
        else if (note < 6)
            pitch >>= 6 - note;
        ch->bend_count = ch->bend_ticks;
        ch->bend_step = (int)(pitch - ((ch->note_pitch << 16) + ch->bend_offset)) / ch->bend_count;
        ch->bend_semitones = 0;
    }
    ch->last_key = ch->key;
    ch->last_transpose = ch->transpose;
}

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

/* Before each note: scans ahead from the channel's position to the next note, following the loops and jumps
 * (C9, CA, EE, EF, F0, F1) on a copy of the loop level, and returns it. CB, CD, D1 and DB end the slur and full
 * length on the way; a rest, A0 or an unused command ends them and the scan (returning A0 for the last two). */
unsigned char seq_look_ahead(SeqChannel *ch)
{
    unsigned char *p = ch->pos;
    unsigned short level = ch->loop_level;
    unsigned int op;
    unsigned char size;
    unsigned int lo;
    short offset;

    for (;;) {
        op = *p;
        if (op < 0x9A) {
            if (op >= 0x8F) {
                ch->glide_ticks = 0;
                ch->note_mode &= ~5;
            }
            return *p;
        }
        if (op < 0xA0)
            return 0xA0;
        size = seq_cmd_scan_sizes[op];
        if (size) {
            p += size;
            continue;
        }
        switch (op) {
        case 0xC9:
            p++;
            if (*p == ch->loop_count[level] + 1) {
                p++;
                level = (level - 1) & 3;
                break;
            }
            p = ch->loop_start[level];
            break;
        case 0xCA:
            p = ch->loop_start[level];
            break;
        case 0xF0:
        case 0xF1:
            p++;
            if (*p == ch->loop_count[level] + 1) {
                p++;
                lo = *p++;
                offset = lo + (*p++ << 8);
                level = (level - 1) & 3;
                p += offset;
                break;
            }
            p += 3;
            break;
        case 0xCB:
        case 0xCD:
        case 0xD1:
        case 0xDB:
            ch->glide_ticks = 0;
            ch->note_mode &= ~5;
            p++;
            break;
        case 0xEE:
            p++;
            lo = *p++;
            offset = lo + (*p++ << 8);
            p += offset;
            break;
        case 0xEF:
            p++;
            lo = *p++;
            if (lo <= seq_song.branch_level) {
                lo = *p++;
                offset = lo + (*p++ << 8);
                p += offset;
                break;
            }
            p += 2;
            break;
        default:
            ch->glide_ticks = 0;
            ch->note_mode &= ~5;
            return 0xA0;
        }
    }
}

/* Walks a stream past every command to the one that ends it (size 0): CA, which loops forever, or anything else, taken
 * as A0. */
unsigned char seq_stream_end(SeqChannel *ch)
{
    unsigned char *p = ch->pos;
    unsigned int op;
    unsigned char size;

    for (;;) {
        op = *p;
        size = seq_cmd_sizes[op];
        p += size;
        if (!size) {
            if (op != 0xCA)
                return 0xA0;
            return 0xCA;
        }
    }
}

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

/* F4 a b: layers a second voice under the channel: the first free one (from 24 in the second song block), or the one
 * it has. The channel takes instrument a and the layer voice instrument b. */
void seq_cmd_layer_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    unsigned int used;
    unsigned int bit = 1;
    unsigned short n;
    unsigned char first;
    unsigned char second;
    unsigned int shift;

    if (!(ch->flags & CHANNEL_LAYER)) {
        used = song->active_voices | song->layer_voices | song->overlap_voices;
        n = D_80062F04 ? 24 : 0;
        for (;;) {
            /* In its own block: as the loop's condition, GCC would rotate the loop, which the game's code isn't. */
            {
                unsigned int free = used & bit;

                if (!free)
                    break;
            }
            bit <<= 1;
            n++;
            if (!(bit & 0xFFFFFF))
                return;
        }
    } else {
        n = ch->layer_voice;
        shift = ch->layer_voice;
        if (ch->layer_voice >= 24)
            shift -= 24;
        bit = 1 << shift;
    }
    if (!(bit & 0xFFFFFF))
        return;
    song->layer_voices |= bit;
    ch->layer_voice = n;
    ch->flags |= CHANNEL_LAYER;
    first = *ch->pos++;
    second = *ch->pos++;
    load_channel_instrument(ch, first);
    load_channel_instrument(&seq_channels[n], second);
}

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

/* B4 d r w: vibrato on, d ticks after each note (an effect takes d as a new depth instead, with no delay), stepping its
 * waveform w every r ticks (0 = 256). The depth becomes an amount scaled by the sounding note's pitch. */
void seq_cmd_vibrato(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    int n;
    int rate;
    unsigned short depth;
    int pitch;
    unsigned short amount;

    ch->flags |= CHANNEL_VIBRATO;
    if (ch->is_effect) {
        ch->vibrato_delay = 0;
        n = *ch->pos++;
        if (n)
            ch->vibrato_depth = n << 8;
    } else {
        ch->vibrato_delay = *ch->pos++;
    }
    rate = *ch->pos++;
    ch->vibrato_rate = rate;
    if (!rate)
        ch->vibrato_rate = 256;
    pitch = (unsigned short)ch->note_pitch;
    ch->vibrato_wave = *ch->pos++;
    depth = ch->vibrato_depth;
    amount = (depth & 0x7F00) >> 8;
    ch->vibrato_amount = (amount * (depth & 0x8000 ? pitch : (pitch * 15) >> 8)) >> 7;
    ch->vibrato_wave_pos = lfo_waveforms[ch->vibrato_wave];
    ch->vibrato_delay_count = ch->vibrato_delay;
    ch->vibrato_step_count = 1;
}

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

/* F8 r: notes take turns between the channel's voice and a second free one, released at rate r. */
void seq_cmd_overlap_on(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    unsigned int used;
    unsigned int bit;
    unsigned short n;

    ch->voice.release_rate = *ch->pos++;
    if (ch->flags & CHANNEL_OVERLAP)
        return;
    n = 0;
    bit = 1;
    used = song->active_voices | song->layer_voices | song->overlap_voices;
    for (; bit & 0xFFFFFF; n++) {
        if (!(used & bit))
            break;
        bit <<= 1;
    }
    if (!(bit & 0xFFFFFF))
        return;
    song->overlap_voices |= bit;
    ch->overlap_voice = n;
    ch->flags |= CHANNEL_OVERLAP;
}

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
    song->beat_tick = 0;
    song->bar_beat = 0;
}

void seq_cmd_bar_number(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->bar_number = *ch->pos++;
    song->bar_number |= *ch->pos++ << 8;
}

void func_800335CC(SeqChannel *ch, SeqSong *song, unsigned int voice)
{
    song->pause_pending = 1;
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
