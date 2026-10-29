#ifndef SOUND_H
#define SOUND_H

/* One sequence channel of the AKAO driver, 0x108 bytes (docs/knowledge_base.md §4). Fields get names as the
 * functions that use them are recovered; the pads are still unread. */
typedef struct SeqChannel {
    unsigned char *pos;        /* 0x00 read pointer into the sequence */
    unsigned char pad_04[0x62];
    unsigned short octave;     /* 0x66 */
    unsigned char pad_68[0x06];
    unsigned short note_mode;  /* 0x6E bit 0 slur (CC), bit 1 this note slurs from the last, bit 2 full length (D0) */
    unsigned char pad_70[0x98];
} SeqChannel;

#endif
