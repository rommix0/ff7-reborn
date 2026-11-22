#ifndef GTE_H
#define GTE_H

/* libgte's types (layouts from Sony's Libref). */
typedef struct VECTOR {
    int vx, vy, vz, pad;
} VECTOR;

typedef struct SVECTOR {
    short vx, vy, vz, pad;
} SVECTOR;

typedef struct MATRIX {
    short m[3][3];
    int t[3];
} MATRIX;

/* The GTE's registers and commands, as libgte's inline macros load and run them. */
#ifdef NATIVE
/* The PC build runs them on its GTE (pc/gte.c), commands by their low 25 bits. */
void gte_cmd(unsigned int op);
unsigned int gte_read(int reg);
void gte_write(int reg, unsigned int v);
void gte_write_ctrl(int reg, unsigned int v);
#define gte_SetRotMatrix(r)                                                                                            \
    (gte_write_ctrl(0, ((unsigned int *)(r))[0]), gte_write_ctrl(1, ((unsigned int *)(r))[1]),                         \
     gte_write_ctrl(2, ((unsigned int *)(r))[2]), gte_write_ctrl(3, ((unsigned int *)(r))[3]),                         \
     gte_write_ctrl(4, ((unsigned int *)(r))[4]))
#define gte_SetTransMatrix(r)                                                                                          \
    (gte_write_ctrl(5, ((unsigned int *)(r))[5]), gte_write_ctrl(6, ((unsigned int *)(r))[6]),                         \
     gte_write_ctrl(7, ((unsigned int *)(r))[7]))
#define gte_ldopv1(v)                                                                                                  \
    (gte_write_ctrl(0, ((unsigned int *)(v))[0]), gte_write_ctrl(2, ((unsigned int *)(v))[1]),                         \
     gte_write_ctrl(4, ((unsigned int *)(v))[2]))
#define gte_ldopv2(v)                                                                                                  \
    (gte_write(11, ((unsigned int *)(v))[2]), gte_write(9, ((unsigned int *)(v))[0]),                                  \
     gte_write(10, ((unsigned int *)(v))[1]))
#define gte_ldv0(v) (gte_write(0, ((unsigned int *)(v))[0]), gte_write(1, ((unsigned int *)(v))[1]))
#define gte_ldlv0(v)                                                                                                   \
    (gte_write(0, ((unsigned short *)(v))[0] | ((unsigned short *)(v))[2] << 16),                                      \
     gte_write(1, ((unsigned int *)(v))[2]))
#define gte_ldclmv(m)                                                                                                  \
    (gte_write(9, ((unsigned short *)(m))[0]), gte_write(10, ((unsigned short *)(m))[3]),                              \
     gte_write(11, ((unsigned short *)(m))[6]))
#define gte_op12() gte_cmd(0x178000C)
#define gte_rtv0() gte_cmd(0x486012)
#define gte_rt() gte_cmd(0x480012)
#define gte_rtir() gte_cmd(0x49E012)
#define gte_stlvnl(v)                                                                                                  \
    (((unsigned int *)(v))[0] = gte_read(25), ((unsigned int *)(v))[1] = gte_read(26),                                 \
     ((unsigned int *)(v))[2] = gte_read(27))
#define gte_stclmv(m)                                                                                                  \
    (((short *)(m))[0] = gte_read(9), ((short *)(m))[3] = gte_read(10), ((short *)(m))[6] = gte_read(11))
#define gte_stlvl(v)                                                                                                   \
    (((unsigned int *)(v))[0] = gte_read(9), ((unsigned int *)(v))[1] = gte_read(10),                                  \
     ((unsigned int *)(v))[2] = gte_read(11))
#else
#define gte_SetRotMatrix(r)                                                                                            \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "lw $13, 0($12)\n"                                                                                \
                     "lw $14, 4($12)\n"                                                                                \
                     "ctc2 $13, $0\n"                                                                                  \
                     "ctc2 $14, $1\n"                                                                                  \
                     "lw $13, 8($12)\n"                                                                                \
                     "lw $14, 12($12)\n"                                                                               \
                     "lw $15, 16($12)\n"                                                                               \
                     "ctc2 $13, $2\n"                                                                                  \
                     "ctc2 $14, $3\n"                                                                                  \
                     "ctc2 $15, $4"                                                                                    \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14", "$15", "memory")
#define gte_SetTransMatrix(r)                                                                                          \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "lw $13, 20($12)\n"                                                                               \
                     "lw $14, 24($12)\n"                                                                               \
                     "ctc2 $13, $5\n"                                                                                  \
                     "lw $15, 28($12)\n"                                                                               \
                     "ctc2 $14, $6\n"                                                                                  \
                     "ctc2 $15, $7"                                                                                    \
                     :                                                                                                 \
                     : "r"(r)                                                                                          \
                     : "$12", "$13", "$14", "$15")
#define gte_ldopv1(v)                                                                                                  \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "lw $13, 0($12)\n"                                                                                \
                     "lw $14, 4($12)\n"                                                                                \
                     "ctc2 $13, $0\n"                                                                                  \
                     "lw $15, 8($12)\n"                                                                                \
                     "ctc2 $14, $2\n"                                                                                  \
                     "ctc2 $15, $4"                                                                                    \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12", "$13", "$14", "$15")
#define gte_ldopv2(v)                                                                                                  \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "lwc2 $11, 8($12)\n"                                                                              \
                     "lwc2 $9, 0($12)\n"                                                                               \
                     "lwc2 $10, 4($12)"                                                                                \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12")
#define gte_ldv0(v)                                                                                                    \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "lwc2 $0, 0($12)\n"                                                                               \
                     "lwc2 $1, 4($12)"                                                                                 \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12")
#define gte_ldlv0(v)                                                                                                   \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "lhu $14, 4($12)\n"                                                                               \
                     "lhu $13, 0($12)\n"                                                                               \
                     "sll $14, $14, 16\n"                                                                              \
                     "or $13, $13, $14\n"                                                                              \
                     "mtc2 $13, $0\n"                                                                                  \
                     "lwc2 $1, 8($12)"                                                                                 \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12", "$13", "$14")
#define gte_ldclmv(m)                                                                                                  \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "lhu $13, 0($12)\n"                                                                               \
                     "lhu $14, 6($12)\n"                                                                               \
                     "lhu $15, 12($12)\n"                                                                              \
                     "mtc2 $13, $9\n"                                                                                  \
                     "mtc2 $14, $10\n"                                                                                 \
                     "mtc2 $15, $11"                                                                                   \
                     :                                                                                                 \
                     : "r"(m)                                                                                          \
                     : "$12", "$13", "$14", "$15")
#define gte_op12() __asm__ volatile("nop\n nop\n .word 0x4B78000C")
#define gte_rtv0() __asm__ volatile("nop\n nop\n .word 0x4A486012")
#define gte_rt() __asm__ volatile("nop\n nop\n .word 0x4A480012" : : : "memory")
#define gte_rtir() __asm__ volatile("nop\n nop\n .word 0x4A49E012" : : : "memory")
#define gte_stlvnl(v)                                                                                                  \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "swc2 $25, 0($12)\n"                                                                              \
                     "swc2 $26, 4($12)\n"                                                                              \
                     "swc2 $27, 8($12)"                                                                                \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12", "memory")
#define gte_stclmv(m)                                                                                                  \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "mfc2 $13, $9\n"                                                                                  \
                     "mfc2 $14, $10\n"                                                                                 \
                     "mfc2 $15, $11\n"                                                                                 \
                     "sh $13, 0($12)\n"                                                                                \
                     "sh $14, 6($12)\n"                                                                                \
                     "sh $15, 12($12)"                                                                                 \
                     :                                                                                                 \
                     : "r"(m)                                                                                          \
                     : "$12", "$13", "$14", "$15", "memory")
#define gte_stlvl(v)                                                                                                   \
    __asm__ volatile("move $12, %0\n"                                                                                  \
                     "swc2 $9, 0($12)\n"                                                                               \
                     "swc2 $10, 4($12)\n"                                                                              \
                     "swc2 $11, 8($12)"                                                                                \
                     :                                                                                                 \
                     : "r"(v)                                                                                          \
                     : "$12", "memory")
#endif

/* r3 = r1 * r2, the rotations column by column and r2's place turned and moved by r1 (libgte's CompMatrix, inline). */
#define gte_CompMatrix(r1, r2, r3)                                                                                     \
    {                                                                                                                  \
        gte_SetRotMatrix(r1);                                                                                          \
        gte_ldclmv(r2);                                                                                                \
        gte_rtir();                                                                                                    \
        gte_stclmv(r3);                                                                                                \
        gte_ldclmv((char *)(r2) + 2);                                                                                  \
        gte_rtir();                                                                                                    \
        gte_stclmv((char *)(r3) + 2);                                                                                  \
        gte_ldclmv((char *)(r2) + 4);                                                                                  \
        gte_rtir();                                                                                                    \
        gte_stclmv((char *)(r3) + 4);                                                                                  \
        gte_SetTransMatrix(r1);                                                                                        \
        gte_ldlv0((char *)(r2) + 20);                                                                                  \
        gte_rt();                                                                                                      \
        gte_stlvl((char *)(r3) + 20);                                                                                  \
    }

#endif
