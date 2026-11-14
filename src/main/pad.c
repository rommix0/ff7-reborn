/* The controllers, 0x8001C434-0x8001CDA4: both pads read through libpad, the Config menu's button mapping, a scripted
 * demo that stands in for pad 1, and the pressed and auto-repeat bits the field and the menus read. */

#include "common.h"
#include "main.h"

/* libpad's receive buffer for one port. */
typedef struct {
    unsigned char status; /* 0xFF: nothing connected */
    unsigned char id;     /* 0x41: a digital pad */
    unsigned char hi;     /* the buttons, active low */
    unsigned char lo;
    unsigned char pad4[30];
} PadBuffer;

/* One pad's buttons: held now, held last frame, newly pressed, and pressed or repeating. */
typedef struct {
    unsigned short held;
    unsigned short prev;
    unsigned short pressed;
    unsigned short repeat;
} PadState;

unsigned char pad_started;
unsigned char demo_text_shown;
short demo_text_x;
short demo_text_y;
unsigned char *demo_script;
int demo_wait; /* frames until the script's next step */
/* Each pad's repeat state under a name of its own: the game reaches each through $gp. */
short field_repeating1;
short field_repeating2;
short field_wait1;
short field_wait2;
short menu_repeating1;
short menu_repeating2;
short menu_wait1;
short menu_wait2;

extern PadBuffer pad_buf[2];
extern PadState field_pad1, field_pad2; /* for the field: repeat after 16 frames, then every 4th */
extern PadState menu_pad1, menu_pad2;   /* for the menus: repeat after 8 frames, then every 2nd */
extern int demo_input;               /* nonzero: pad 1 comes from demo_script */
extern unsigned char button_map[16]; /* the Config menu's mapping: by bit, the bit it becomes */

void StartPAD(void);
void InitPAD(void *buf1, int len1, void *buf2, int len2);
unsigned char func_8001F6B4(void);
void func_8001F6C0(unsigned char *text, int k);
void func_8001F6E4(int show, int x, int y);
int func_80023050(void);
unsigned short func_8001C788(void);

/* Starts libpad the first time; the demo is off. */
void func_8001C434(void)
{
    if (!pad_started) {
        pad_started = 1;
        StartPAD();
        InitPAD(&pad_buf[0], 4, &pad_buf[1], 4);
    }
    demo_input = 0;
}

/* Starts a demo script. */
void func_8001C484(unsigned char *script)
{
    demo_script = script;
    demo_wait = 20;
}

/* Pad 1's buttons, 0 unless a digital pad is there. */
unsigned short func_8001C498(void)
{
    int buttons;

    if (pad_buf[0].status != 0xFF) {
        if (pad_buf[0].id == 0x41)
            buttons = ~(pad_buf[0].hi << 8 | pad_buf[0].lo);
        else
            buttons = 0;
    } else
        buttons = 0;
    return buttons;
}

/* Shows the script's text and returns what follows it: the text ends at 0xFF, and 0xFA-0xFE take a second byte. */
unsigned char *func_8001C4E8(unsigned char *text)
{
    int c, i;

    demo_text_shown = 1;
    func_8001F6E4(1, demo_text_x, demo_text_y);
    func_8001F6C0(text, 7);
    for (i = 0; i < 256; i++) {
        c = *text;
        if (c < 0xFF) {
            if (c >= 0xFA)
                text += 2;
            else
                text++;
        } else
            text++;
        if (*text == 0xFF) {
            text++;
            break;
        }
    }
    return text;
}

/* The demo's text is done once its window has closed. */
void func_8001C58C(void)
{
    if (!func_8001F6B4())
        demo_text_shown = 0;
}

/* The script's next step: the buttons to press, or a wait, text, its place or the end. */
int func_8001C5BC(void)
{
    int buttons = 0;

    if (demo_text_shown)
        func_8001C58C();
    else
        switch (*demo_script++) {
        case 0:
            demo_wait = *demo_script++;
            demo_wait += *demo_script << 8;
            demo_script++;
            break;
        case 2:
            buttons = 0x1000;
            demo_wait = 20;
            break;
        case 3:
            buttons = 0x4000;
            demo_wait = 20;
            break;
        case 4:
            buttons = 0x8000;
            demo_wait = 20;
            break;
        case 5:
            buttons = 0x2000;
            demo_wait = 20;
            break;
        case 8:
            buttons = 0x80;
            demo_wait = 20;
            break;
        case 9:
            buttons = 0x20;
            demo_wait = 20;
            break;
        case 6:
            buttons = 0x10;
            demo_wait = 20;
            break;
        case 7:
            buttons = 0x40;
            demo_wait = 20;
            break;
        case 10:
            buttons = 0x8;
            demo_wait = 20;
            break;
        case 11:
            buttons = 0x2;
            demo_wait = 20;
            break;
        case 12:
            buttons = 0x4;
            demo_wait = 20;
            break;
        case 13:
            buttons = 0x1;
            demo_wait = 20;
            break;
        case 16:
            demo_script = func_8001C4E8(demo_script);
            demo_wait = 80;
            break;
        case 18:
            demo_text_x = *demo_script++;
            demo_text_x += *demo_script << 8;
            demo_script++;
            demo_text_y = *demo_script++;
            demo_text_y += *demo_script << 8;
            demo_script++;
            demo_wait = 60;
            break;
        case 17:
            func_8001F6E4(0, 0, 0);
            demo_input = 0;
            demo_wait = 0;
            break;
        }
    return buttons;
}

/* The demo's buttons this frame; it waits while a disc read is going. */
unsigned short func_8001C788(void)
{
    int buttons = 0;

    if (demo_wait)
        demo_wait--;
    else if (!func_80023050() || func_80023050() == 1) {
        if (!cd_step())
            buttons = func_8001C5BC();
    }
    return buttons;
}

/* Both pads' buttons, pad 2 in the high half. */
unsigned int func_8001C808(void)
{
    unsigned short pad1, pad2;

    if (!demo_input) {
        if (pad_buf[0].status != 0xFF) {
            if (pad_buf[0].id == 0x41)
                pad1 = ~(pad_buf[0].lo | pad_buf[0].hi << 8);
            else
                pad1 = 0;
        } else
            pad1 = 0;
    } else
        pad1 = func_8001C788();
    if (pad_buf[1].status != 0xFF) {
        if (pad_buf[1].id == 0x41)
            pad2 = ~(pad_buf[1].lo | pad_buf[1].hi << 8);
        else
            pad2 = 0;
    } else
        pad2 = 0;
    return pad1 | pad2 << 16;
}

/* Both pads, pad 1 through the Config menu's mapping. */
unsigned int func_8001C8D4(void)
{
    unsigned int raw = func_8001C808();
    unsigned int pad2 = raw & 0xFFFF0000;
    unsigned int buttons;
    int i;

    if ((config >> 2 & 3) && !demo_input) {
        buttons = 0;
        for (i = 0; i < 16; i++)
            if (raw & 1 << i)
                buttons |= 1 << button_map[i];
    } else
        buttons = raw & 0xFFFF;
    return buttons | pad2;
}

/* The menus' pressed and repeat bits, from the buttons func_8001CB48 read. */
void func_8001C980(void)
{
    menu_pad2.held = field_pad2.held;
    menu_pad1.held = field_pad1.held;
    menu_pad1.pressed = menu_pad1.held ^ menu_pad1.prev;
    menu_pad1.pressed &= menu_pad1.held;
    if (menu_pad1.held != menu_pad1.prev) {
        menu_repeating1 = 0;
        menu_wait1 = 0;
    } else if (menu_repeating1) {
        if (menu_wait1 == 1) {
            menu_pad1.repeat = menu_pad1.held;
            menu_wait1 = 0;
        } else {
            menu_wait1++;
            menu_pad1.repeat = 0;
        }
    } else {
        if (menu_wait1 == 7) {
            menu_repeating1 = 1;
            menu_wait1 = 0;
        } else
            menu_wait1++;
        menu_pad1.repeat = 0;
    }
    menu_pad2.pressed = menu_pad2.held ^ menu_pad2.prev;
    menu_pad2.pressed &= menu_pad2.held;
    if (menu_pad2.held != menu_pad2.prev) {
        menu_repeating2 = 0;
        menu_wait2 = 0;
    } else if (menu_repeating2) {
        if (menu_wait2 == 1) {
            menu_pad2.repeat = menu_pad2.held;
            menu_wait2 = 0;
        } else {
            menu_wait2++;
            menu_pad2.repeat = 0;
        }
    } else {
        if (menu_wait2 == 7) {
            menu_repeating2 = 1;
            menu_wait2 = 0;
        } else
            menu_wait2++;
        menu_pad2.repeat = 0;
    }
    menu_pad1.repeat |= menu_pad1.pressed;
    menu_pad2.repeat |= menu_pad2.pressed;
    menu_pad1.prev = menu_pad1.held;
    menu_pad2.prev = menu_pad2.held;
}

/* Reads both pads for the frame: held, pressed and repeat bits for the field. */
void func_8001CB48(void)
{
    unsigned int raw = func_8001C808();
    int i;

    field_pad2.held = raw >> 16;
    if ((config >> 2 & 3) && !demo_input) {
        field_pad1.held = 0;
        for (i = 0; i < 16; i++)
            if (raw & 1 << i)
                field_pad1.held |= 1 << button_map[i];
    } else
        field_pad1.held = raw;
    field_pad1.repeat = 0;
    field_pad1.pressed = field_pad1.held ^ field_pad1.prev;
    field_pad1.pressed &= field_pad1.held;
    if (field_pad1.held != field_pad1.prev) {
        field_wait1 = 0;
        field_repeating1 = 0;
    } else if (field_repeating1) {
        if (field_wait1 == 3) {
            field_pad1.repeat = field_pad1.held;
            field_wait1 = 0;
        } else
            field_wait1++;
    } else if (field_wait1 == 15) {
        field_repeating1 = 1;
        field_wait1 = 0;
    } else
        field_wait1++;
    field_pad2.pressed = field_pad2.held ^ field_pad2.prev;
    field_pad2.pressed &= field_pad2.held;
    if (field_pad2.held != field_pad2.prev) {
        field_repeating2 = 0;
        field_wait2 = 0;
    } else if (field_repeating2) {
        if (field_wait2 == 3) {
            field_pad2.repeat = field_pad2.held;
            field_wait2 = 0;
        } else {
            field_wait2++;
            field_pad2.repeat = 0;
        }
    } else {
        if (field_wait2 == 15) {
            field_repeating2 = 1;
            field_wait2 = 0;
        } else
            field_wait2++;
        field_pad2.repeat = 0;
    }
    field_pad1.repeat |= field_pad1.pressed;
    field_pad2.repeat |= field_pad2.pressed;
    field_pad1.prev = field_pad1.held;
    field_pad2.prev = field_pad2.held;
}
