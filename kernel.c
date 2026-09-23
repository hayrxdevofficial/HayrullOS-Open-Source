#include "io.h"

#define SCREEN_W 320
#define SCREEN_H 200
#define VID_MEM  ((unsigned char*)0xA0000)

#define TASKBAR_H 16
#define TASKBAR_Y (SCREEN_H - TASKBAR_H)

#define COLOR_BLACK        0
#define COLOR_BLUE         1
#define COLOR_GREEN        2
#define COLOR_RED          4
#define COLOR_LIGHT_GRAY   7
#define COLOR_DARK_GRAY    8
#define COLOR_LIGHT_BLUE   9
#define COLOR_LIGHT_GREEN  10
#define COLOR_LIGHT_RED    12
#define COLOR_YELLOW       14
#define COLOR_WHITE        15

#define PS2_DATA   0x60
#define PS2_STATUS 0x64
#define PS2_CMD    0x64
#define CMOS_ADDR  0x70
#define CMOS_DATA  0x71

#define ICON_W 32
#define ICON_H 32
#define ICON1_X 12
#define ICON1_Y 10
#define ICON2_X 56
#define ICON2_Y 10

#define WIN_HEADER_H 12
#define WIN_MIN_W 140
#define WIN_MIN_H 70
#define CON_COLS 37
#define CON_ROWS 14
#define ED_COLS  37
#define ED_ROWS  14

#define CURSOR_W 8
#define CURSOR_H 8

#define APP_NONE    0
#define APP_CONSOLE 1
#define APP_EDITOR  2

static volatile int active_app = APP_CONSOLE;

static volatile int con_open = 1;
static volatile int con_x = 40, con_y = 24, con_w = 240, con_h = 120;
static volatile int con_dragging = 0, con_drag_off_x = 0, con_drag_off_y = 0;
static volatile int con_resizing = 0;
static volatile int con_rs_x = 0, con_rs_y = 0, con_rs_w = 0, con_rs_h = 0;

static volatile int ed_open = 0;
static volatile int ed_x = 40, ed_y = 24, ed_w = 240, ed_h = 120;
static volatile int ed_dragging = 0, ed_drag_off_x = 0, ed_drag_off_y = 0;

static char console_buf[CON_ROWS][CON_COLS];
static volatile int con_cur_row = 0;
static volatile int con_cur_col = 0;
static char input_line[64];
static volatile int input_pos = 0;

static char editor_buf[ED_ROWS][ED_COLS];
static volatile int ed_cur_row = 0;
static volatile int ed_cur_col = 0;

static volatile int mouse_x = 160, mouse_y = 100;
static unsigned char mouse_cycle = 0;
static unsigned char mouse_byte[3];
static unsigned char prev_buttons = 0;
static volatile int mouse_clicked = 0;
static volatile int mouse_released = 0;

static const unsigned char cursor_sprite[CURSOR_H][CURSOR_W] = {
    {2,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0},
    {2,1,1,2,0,0,0,0},
    {2,1,1,1,2,0,0,0},
    {2,1,1,1,1,2,0,0},
    {2,1,2,2,2,2,2,0},
    {2,2,0,2,1,1,2,0},
};

static volatile unsigned int tick = 0;
static volatile int cursor_blink = 1;
static volatile int needs_redraw = 1;
static volatile int last_shown_min = -1;

static const char font_chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789:-._<>[]()!?";

static const unsigned char font_data[][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00},
    {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
    {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
    {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x7E,0x00},
    {0x7E,0x60,0x60,0x78,0x60,0x60,0x60,0x00},
    {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00},
    {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
    {0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x6C,0x38,0x00},
    {0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00},
    {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
    {0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00},
    {0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00},
    {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
    {0x3C,0x66,0x66,0x66,0x66,0x3C,0x0E,0x00},
    {0x7C,0x66,0x66,0x7C,0x78,0x6C,0x66,0x00},
    {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},
    {0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00},
    {0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
    {0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00},
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
    {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x3C,0x00},
    {0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00},
    {0x3C,0x66,0x6E,0x7E,0x76,0x66,0x3C,0x00},
    {0x18,0x18,0x38,0x18,0x18,0x18,0x7E,0x00},
    {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00},
    {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
    {0x06,0x0E,0x1E,0x66,0x7F,0x06,0x06,0x00},
    {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
    {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00},
    {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00},
    {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
    {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    {0x00,0x00,0x30,0x18,0x0C,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x00,0x00,0x18,0x18,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x0C,0x18,0x30,0x00,0x00,0x00},
    {0x18,0x3C,0x66,0x66,0x66,0x3C,0x18,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    {0x18,0x18,0x00,0x18,0x18,0x00,0x18,0x00},
};

static const unsigned char *font_get(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    for (int i = 0; font_chars[i]; i++)
        if (font_chars[i] == c) return font_data[i];
    return font_data[0];
}

static void putpixel(int x, int y, unsigned char color) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    VID_MEM[y * SCREEN_W + x] = color;
}

static void fill_rect(int x, int y, int w, int h, unsigned char color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SCREEN_W) w = SCREEN_W - x;
    if (y + h > SCREEN_H) h = SCREEN_H - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; yy++) {
        unsigned char *row = VID_MEM + yy * SCREEN_W + x;
        for (int xx = 0; xx < w; xx++) row[xx] = color;
    }
}

static void draw_char(int x, int y, char c, unsigned char fg) {
    const unsigned char *g = font_get(c);
    for (int row = 0; row < 8; row++) {
        unsigned char bits = g[row];
        for (int col = 0; col < 8; col++)
            if (bits & (0x80 >> col))
                putpixel(x + col, y + row, fg);
    }
}

static void draw_string(int x, int y, const char *str, unsigned char fg) {
    while (*str) {
        draw_char(x, y, *str, fg);
        x += 8;
        str++;
    }
}

static void con_clear(void) {
    for (int r = 0; r < CON_ROWS; r++)
        for (int c = 0; c < CON_COLS; c++)
            console_buf[r][c] = ' ';
    con_cur_row = 0;
    con_cur_col = 0;
    input_pos = 0;
}

static void con_scroll(void) {
    for (int r = 0; r < CON_ROWS - 1; r++)
        for (int c = 0; c < CON_COLS; c++)
            console_buf[r][c] = console_buf[r + 1][c];
    for (int c = 0; c < CON_COLS; c++)
        console_buf[CON_ROWS - 1][c] = ' ';
    con_cur_row = CON_ROWS - 1;
}

static void con_putchar(char ch) {
    if (ch == '\n') {
        con_cur_col = 0;
        con_cur_row++;
        if (con_cur_row >= CON_ROWS) con_scroll();
        return;
    }
    if (con_cur_col >= CON_COLS) {
        con_cur_col = 0;
        con_cur_row++;
        if (con_cur_row >= CON_ROWS) con_scroll();
    }
    console_buf[con_cur_row][con_cur_col] = ch;
    con_cur_col++;
}

static void con_print(const char *s) {
    while (*s) con_putchar(*s++);
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static int str_starts(const char *a, const char *b) {
    while (*b) { if (*a != *b) return 0; a++; b++; }
    return 1;
}

static unsigned char cmos_read(unsigned char reg) {
    outb(CMOS_ADDR, reg);
    return inb(CMOS_DATA);
}

static unsigned char bcd2bin(unsigned char b) {
    return (unsigned char)((b & 0x0F) + ((b >> 4) * 10));
}

static void read_time(int *h, int *m) {
    while (cmos_read(0x0A) & 0x80);
    unsigned char hh = cmos_read(0x04);
    unsigned char mm = cmos_read(0x02);
    unsigned char rB = cmos_read(0x0B);
    if (!(rB & 0x04)) { hh = bcd2bin(hh); mm = bcd2bin(mm); }
    *h = hh & 0x7F;
    *m = mm;
}

static void power_off(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    while (1) __asm__ volatile ("hlt");
}

static void cmd_help(void) {
    con_print("HELP CLEAR TIME ABOUT\n");
    con_print("ECHO SHUTDOWN\n");
}

static void cmd_time(void) {
    int h, m;
    read_time(&h, &m);
    char buf[6];
    buf[0] = '0' + (h / 10);
    buf[1] = '0' + (h % 10);
    buf[2] = ':';
    buf[3] = '0' + (m / 10);
    buf[4] = '0' + (m % 10);
    buf[5] = 0;
    con_print("TIME: ");
    con_print(buf);
    con_putchar('\n');
}

static void execute_command(void) {
    input_line[input_pos] = 0;
    con_putchar('\n');

    if (input_pos == 0) { con_print("> "); input_pos = 0; return; }

    if (str_eq(input_line, "help")) cmd_help();
    else if (str_eq(input_line, "clear")) con_clear();
    else if (str_eq(input_line, "time")) cmd_time();
    else if (str_eq(input_line, "about")) con_print("HayrullOS V0.8\n");
    else if (str_eq(input_line, "shutdown")) power_off();
    else if (str_starts(input_line, "echo ")) {
        const char *p = input_line + 5;
        while (*p) con_putchar(*p++);
        con_putchar('\n');
    } else {
        con_print("UNKNOWN: ");
        con_print(input_line);
        con_putchar('\n');
    }

    con_print("> ");
    input_pos = 0;
}

static const char kbd_map[128] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0
};

static void kbd_in(unsigned char sc) {
    if (sc & 0x80) return;

    if (active_app == APP_CONSOLE && con_open) {
        if (sc == 0x1C) { execute_command(); needs_redraw = 1; return; }
        if (sc == 0x0E) {
            if (input_pos > 0) {
                input_pos--;
                con_cur_col--;
                console_buf[con_cur_row][con_cur_col] = ' ';
                needs_redraw = 1;
            }
            return;
        }
        char c = kbd_map[sc];
        if (c == 0) return;
        if (input_pos < CON_COLS - 1 && input_pos < 63) {
            input_line[input_pos++] = c;
            con_putchar(c);
            needs_redraw = 1;
        }
        return;
    }

    if (active_app == APP_EDITOR && ed_open) {
        if (sc == 0x0E) {
            if (ed_cur_col > 0) {
                ed_cur_col--;
                editor_buf[ed_cur_row][ed_cur_col] = ' ';
                needs_redraw = 1;
            }
            return;
        }
        if (sc == 0x1C) {
            ed_cur_col = 0;
            ed_cur_row++;
            if (ed_cur_row >= ED_ROWS) ed_cur_row = ED_ROWS - 1;
            needs_redraw = 1;
            return;
        }
        char c = kbd_map[sc];
        if (c == 0) return;
        if (ed_cur_col < ED_COLS - 1) {
            editor_buf[ed_cur_row][ed_cur_col] = c;
            ed_cur_col++;
            needs_redraw = 1;
        }
    }
}

static void ps2_ww(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(PS2_STATUS) & 0x02)) return;
}

static void ps2_wr(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(PS2_STATUS) & 0x01) return;
}

static void mouse_write(unsigned char v) {
    ps2_ww();
    outb(PS2_CMD, 0xD4);
    ps2_ww();
    outb(PS2_DATA, v);
}

static unsigned char mouse_read(void) {
    ps2_wr();
    return inb(PS2_DATA);
}

static void mouse_init(void) {
    ps2_ww(); outb(PS2_CMD, 0xA8);
    ps2_ww(); outb(PS2_CMD, 0x20);
    ps2_wr();
    unsigned char s = inb(PS2_DATA);
    s |= 0x02; s &= ~0x20;
    ps2_ww(); outb(PS2_CMD, 0x60);
    ps2_ww(); outb(PS2_DATA, s);
    mouse_write(0xF6); mouse_read();
    mouse_write(0xF4); mouse_read();
}

static void mouse_in(unsigned char data) {
    if (mouse_cycle == 0 && !(data & 0x08)) return;
    mouse_byte[mouse_cycle++] = data;
    if (mouse_cycle < 3) return;
    mouse_cycle = 0;

    unsigned char flags = mouse_byte[0];
    if (flags & 0xC0) return;

    unsigned char buttons = flags & 0x07;
    if ((buttons & 0x01) && !(prev_buttons & 0x01)) mouse_clicked = 1;
    if (!(buttons & 0x01) && (prev_buttons & 0x01)) mouse_released = 1;
    prev_buttons = buttons;

    int dx = mouse_byte[1];
    int dy = mouse_byte[2];
    if (flags & 0x10) dx |= 0xFFFFFF00;
    if (flags & 0x20) dy |= 0xFFFFFF00;

    if (dx || dy) needs_redraw = 1;

    mouse_x += dx;
    mouse_y -= dy;
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x > SCREEN_W - CURSOR_W) mouse_x = SCREEN_W - CURSOR_W;
    if (mouse_y > SCREEN_H - CURSOR_H) mouse_y = SCREEN_H - CURSOR_H;
}

static void ps2_poll(void) {
    while (inb(PS2_STATUS) & 0x01) {
        unsigned char st = inb(PS2_STATUS);
        unsigned char d = inb(PS2_DATA);
        if (st & 0x20) mouse_in(d);
        else kbd_in(d);
    }
}

static int pt_rect(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static void on_click(int mx, int my) {
    if (pt_rect(mx, my, SCREEN_W - 22, TASKBAR_Y + 2, 18, TASKBAR_H - 4))
        power_off();

    if (pt_rect(mx, my, 4, TASKBAR_Y + 2, 72, TASKBAR_H - 4)) {
        active_app = APP_CONSOLE;
        con_open = 1;
        needs_redraw = 1;
        return;
    }
    if (pt_rect(mx, my, 80, TASKBAR_Y + 2, 72, TASKBAR_H - 4)) {
        active_app = APP_EDITOR;
        ed_open = 1;
        needs_redraw = 1;
        return;
    }

    if (pt_rect(mx, my, ICON1_X, ICON1_Y, ICON_W, ICON_H + 10)) {
        active_app = APP_CONSOLE;
        con_open = 1;
        needs_redraw = 1;
        return;
    }
    if (pt_rect(mx, my, ICON2_X, ICON2_Y, ICON_W, ICON_H + 10)) {
        active_app = APP_EDITOR;
        ed_open = 1;
        needs_redraw = 1;
        return;
    }

    if (con_open) {
        if (pt_rect(mx, my, con_x + con_w - 12, con_y + 3, 10, 10)) {
            con_open = 0; needs_redraw = 1; return;
        }
        if (pt_rect(mx, my, con_x + con_w - 24, con_y + 3, 10, 10)) {
            con_open = 0; needs_redraw = 1; return;
        }
        if (pt_rect(mx, my, con_x + con_w - 10, con_y + con_h - 10, 10, 10)) {
            con_resizing = 1;
            con_rs_x = mx; con_rs_y = my;
            con_rs_w = con_w; con_rs_h = con_h;
            return;
        }
        if (pt_rect(mx, my, con_x, con_y, con_w, WIN_HEADER_H)) {
            con_dragging = 1;
            con_drag_off_x = mx - con_x;
            con_drag_off_y = my - con_y;
            active_app = APP_CONSOLE;
            needs_redraw = 1;
            return;
        }
    }
    if (ed_open) {
        if (pt_rect(mx, my, ed_x + ed_w - 12, ed_y + 3, 10, 10)) {
            ed_open = 0; needs_redraw = 1; return;
        }
        if (pt_rect(mx, my, ed_x, ed_y, ed_w, WIN_HEADER_H)) {
            ed_dragging = 1;
            ed_drag_off_x = mx - ed_x;
            ed_drag_off_y = my - ed_y;
            active_app = APP_EDITOR;
            needs_redraw = 1;
        }
    }
}

static void on_release(void) {
    con_dragging = 0;
    con_resizing = 0;
    ed_dragging = 0;
}

static void on_move(void) {
    if (con_dragging) {
        int nx = mouse_x - con_drag_off_x;
        int ny = mouse_y - con_drag_off_y;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx + con_w > SCREEN_W) nx = SCREEN_W - con_w;
        if (ny + con_h > TASKBAR_Y) ny = TASKBAR_Y - con_h;
        con_x = nx; con_y = ny;
        needs_redraw = 1;
    } else if (con_resizing) {
        int nw = con_rs_w + (mouse_x - con_rs_x);
        int nh = con_rs_h + (mouse_y - con_rs_y);
        if (nw < WIN_MIN_W) nw = WIN_MIN_W;
        if (nh < WIN_MIN_H) nh = WIN_MIN_H;
        if (con_x + nw > SCREEN_W) nw = SCREEN_W - con_x;
        if (con_y + nh > TASKBAR_Y) nh = TASKBAR_Y - con_y;
        con_w = nw; con_h = nh;
        needs_redraw = 1;
    } else if (ed_dragging) {
        int nx = mouse_x - ed_drag_off_x;
        int ny = mouse_y - ed_drag_off_y;
        if (nx < 0) nx = 0;
        if (ny < 0) ny = 0;
        if (nx + ed_w > SCREEN_W) nx = SCREEN_W - ed_w;
        if (ny + ed_h > TASKBAR_Y) ny = TASKBAR_Y - ed_h;
        ed_x = nx; ed_y = ny;
        needs_redraw = 1;
    }
}

static void draw_wallpaper(void) {
    for (int y = 0; y < TASKBAR_Y; y++)
        for (int x = 0; x < SCREEN_W; x++)
            VID_MEM[y * SCREEN_W + x] = COLOR_BLUE;
}

static void draw_icon(int x, int y, const char *label, char c1, char c2) {
    fill_rect(x + 2, y + 2, ICON_W, ICON_H, COLOR_DARK_GRAY);
    fill_rect(x, y, ICON_W, ICON_H, COLOR_WHITE);
    for (int i = 0; i < ICON_W; i++) { putpixel(x + i, y, COLOR_BLACK); putpixel(x + i, y + ICON_H - 1, COLOR_BLACK); }
    for (int i = 0; i < ICON_H; i++) { putpixel(x, y + i, COLOR_BLACK); putpixel(x + ICON_W - 1, y + i, COLOR_BLACK); }
    fill_rect(x + 1, y + 1, ICON_W - 2, 6, COLOR_LIGHT_GRAY);
    draw_char(x + 6, y + 12, c1, COLOR_BLACK);
    draw_char(x + 14, y + 12, c2, COLOR_BLACK);
    draw_string(x, y + ICON_H +
