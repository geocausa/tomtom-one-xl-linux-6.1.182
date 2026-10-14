/*
 * Comprehensive Handheld Application Shell & Dashboard (ttlauncher)
 * Designed for TomTom ONE XL (Samsung S3C2412 - 480x272 LTE430WQ)
 *
 * Applications Included:
 * 1. CALIBRATE: 4-point touch calibration with persistent /etc/pointercal & /sdcard/pointercal
 * 2. TOOLS: Terminal plus USB Network & SSH
 * 3. EXPLORER: Directory browser & text file viewer for rootfs and SD card
 * 4. BLUETOOTH: A2DP sink manager with phone-controlled volume
 * POWER: System Power Management (Reboot, Power Off via GPE3 suicide, Cancel)
 */

#include "tinylib.h"
#include "font8x16.h"

#define FB_WIDTH  480
#define FB_HEIGHT 272

#define RGB565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

#define COLOR_BG        RGB565(18, 22, 32)
#define COLOR_TOPBAR    RGB565(10, 14, 22)
#define COLOR_BAR_LINE  RGB565(0, 150, 255)
#define COLOR_BLACK     RGB565(0, 0, 0)
#define COLOR_WHITE     RGB565(255, 255, 255)
#define COLOR_GRAY      RGB565(130, 140, 155)
#define COLOR_DARKGRAY  RGB565(35, 42, 58)
#define COLOR_BLUE      RGB565(0, 140, 255)
#define COLOR_CYAN      RGB565(0, 215, 235)
#define COLOR_GREEN     RGB565(45, 215, 85)
#define COLOR_RED       RGB565(255, 65, 65)
#define COLOR_YELLOW    RGB565(255, 205, 45)
#define COLOR_ORANGE    RGB565(255, 140, 30)
#define COLOR_PURPLE    RGB565(185, 75, 245)

#define MAX_EV_FDS 8
static int ev_fds[MAX_EV_FDS];
static int num_ev_fds = 0;
static int pwr_button_down = 0;

/* Static BSS buffers for sub-applications (avoids large stack frames) */
static char term_input_buf[128];
static char explorer_current_dir[128];
static char explorer_entries[32][64];
static int explorer_is_dir[32];
static char explorer_dbuf[1536];
static char viewer_buf[2048];

static void show_power_menu(void);

static int opened_event_indices[6] = {0};

static void init_input(void) {
    for (int i = 0; i < 6; i++) {
        if (opened_event_indices[i]) continue;
        if (num_ev_fds >= MAX_EV_FDS) break;
        char path[] = "/dev/input/event0";
        path[16] = '0' + i;
        int fd = sys_open(path, O_RDONLY | O_NONBLOCK, 0);
        if (fd >= 0) {
            ev_fds[num_ev_fds++] = fd;
            opened_event_indices[i] = 1;
        }
    }
}

static int read_input_event(struct input_event *ev) {
    for (int i = 0; i < num_ev_fds; i++) {
        if (sys_read(ev_fds[i], ev, sizeof(*ev)) == sizeof(*ev)) {
            return 1;
        }
    }
    return 0;
}

static void flush_input(void) {
    struct input_event ev;
    while (read_input_event(&ev)) {
        if (ev.type == EV_KEY && ev.code == 116) {
            pwr_button_down = ev.value;
        }
    }
}

static int fb_fd = -1;
static unsigned short back_buffer[FB_WIDTH * FB_HEIGHT];
static int calib_min_x = 880;
static int calib_max_x = 120;
static int calib_min_y = 880;
static int calib_max_y = 120;
static int is_calibrated = 0;

static int cur_raw_x = 0;
static int cur_raw_y = 0;
static int cur_px_x = 0;
static int cur_px_y = 0;
static int last_key_code = 0;
static int last_key_val = 0;

/* Framebuffer Primitives */
static void fb_flip(void) {
    if (fb_fd >= 0) {
        sys_lseek(fb_fd, 0, SEEK_SET);
        sys_write(fb_fd, back_buffer, FB_WIDTH * FB_HEIGHT * 2);
    }
}

static void draw_pixel(int x, int y, unsigned short color) {
    if (x >= 0 && x < FB_WIDTH && y >= 0 && y < FB_HEIGHT) {
        back_buffer[y * FB_WIDTH + x] = color;
    }
}

static void draw_rect(int x, int y, int w, int h, unsigned short color) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            draw_pixel(i, j, color);
        }
    }
}

static void draw_char(int x, int y, char c, unsigned short fg, unsigned short bg) {
    if (c < 32 || c > 126) c = ' ';
    const unsigned char *glyph = font8x16_data[(unsigned char)c];
    for (int row = 0; row < 16; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                draw_pixel(x + col, y + row, fg);
            } else if (bg != 0xFFFF) {
                draw_pixel(x + col, y + row, bg);
            }
        }
    }
}

static void draw_string(int x, int y, const char *str, unsigned short fg, unsigned short bg) {
    while (*str) {
        draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

static void draw_num(int x, int y, int val, unsigned short fg, unsigned short bg) {
    char buf[16];
    int pos = 0;
    if (val < 0) {
        buf[pos++] = '-';
        val = -val;
    }
    if (val == 0) {
        buf[pos++] = '0';
    } else {
        char temp[12];
        int tpos = 0;
        while (val > 0) {
            temp[tpos++] = '0' + int_mod(val, 10);
            val = int_div(val, 10);
        }
        while (tpos > 0) {
            buf[pos++] = temp[--tpos];
        }
    }
    buf[pos] = '\0';
    draw_string(x, y, buf, fg, bg);
}

static int parse_int(const char *s, int *out_pos) {
    int val = 0;
    int sign = 1;
    int p = 0;
    while (s[p] == ' ' || s[p] == '\t' || s[p] == '\n') p++;
    if (s[p] == '-') { sign = -1; p++; }
    while (s[p] >= '0' && s[p] <= '9') {
        val = val * 10 + (s[p] - '0');
        p++;
    }
    if (out_pos) *out_pos = p;
    return val * sign;
}

/* Calibration Loading & Coordinate Mapping */
static void load_calibration(void) {
    int fd = sys_open("/sdcard/pointercal", O_RDONLY, 0);
    if (fd < 0) fd = sys_open("/etc/pointercal", O_RDONLY, 0);
    if (fd >= 0) {
        char buf[128];
        long n = sys_read(fd, buf, sizeof(buf) - 1);
        sys_close(fd);
        if (n > 0) {
            buf[n] = '\0';
            int p = 0, shift = 0;
            calib_min_x = parse_int(buf + p, &shift); p += shift;
            calib_max_x = parse_int(buf + p, &shift); p += shift;
            calib_min_y = parse_int(buf + p, &shift); p += shift;
            calib_max_y = parse_int(buf + p, &shift);
            is_calibrated = 1;
            return;
        }
    }
    /* Default initial values for TomTom ONE XL */
    calib_min_x = 880;
    calib_max_x = 120;
    calib_min_y = 880;
    calib_max_y = 120;
    is_calibrated = 0;
}

static void save_calibration(void) {
    char buf[128];
    int pos = 0;
    char temp[16];

    #define APPEND_INT(v) do { \
        int _v = (v); \
        if (_v < 0) { buf[pos++] = '-'; _v = -_v; } \
        int _tp = 0; \
        if (_v == 0) temp[_tp++] = '0'; \
        while (_v > 0) { temp[_tp++] = '0' + int_mod(_v, 10); _v = int_div(_v, 10); } \
        while (_tp > 0) buf[pos++] = temp[--_tp]; \
        buf[pos++] = ' '; \
    } while (0)

    APPEND_INT(calib_min_x);
    APPEND_INT(calib_max_x);
    APPEND_INT(calib_min_y);
    APPEND_INT(calib_max_y);
    buf[pos - 1] = '\n';

    int fd1 = sys_open("/etc/pointercal", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd1 >= 0) {
        sys_write(fd1, buf, pos);
        sys_close(fd1);
    }
    int fd2 = sys_open("/sdcard/pointercal", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd2 >= 0) {
        sys_write(fd2, buf, pos);
        sys_close(fd2);
    }
    is_calibrated = 1;
}

static void map_touch(int rx, int ry, int *out_x, int *out_y) {
    int dx = calib_max_x - calib_min_x;
    int dy = calib_max_y - calib_min_y;
    if (dx == 0) dx = 1;
    if (dy == 0) dy = 1;

    int px = int_div((rx - calib_min_x) * FB_WIDTH, dx);
    int py = int_div((ry - calib_min_y) * FB_HEIGHT, dy);

    if (px < 0) px = 0;
    if (px >= FB_WIDTH) px = FB_WIDTH - 1;
    if (py < 0) py = 0;
    if (py >= FB_HEIGHT) py = FB_HEIGHT - 1;

    *out_x = px;
    *out_y = py;
}

/* ========================================================================= */
/* Unified Touch & Key Event Handler (Reliable Debounced Clicks)              */
/* ========================================================================= */
static int touch_rx = -1, touch_ry = -1;
static int touch_down = 0;
static int touch_was_down = 0;

static void reset_touch_state(void) {
    flush_input();
    touch_was_down = 0;
    touch_down = 0;
    touch_rx = -1;
    touch_ry = -1;
}

/* Returns:
 * 1 = Click / Touch Down (*out_px, *out_py updated)
 * 2 = Hardware Power Button pressed
 * 0 = No click event
 */
static int poll_touch_click(int *out_px, int *out_py) {
    struct input_event ev;
    while (read_input_event(&ev)) {
        if (ev.type == EV_KEY) {
            last_key_code = ev.code;
            last_key_val = ev.value;
            if (ev.code == 116 /* KEY_POWER */ || ev.code == 142 /* KEY_SLEEP */) {
                if (ev.value == 1) {
                    if (!pwr_button_down) {
                        pwr_button_down = 1;
                        return 2;
                    }
                } else if (ev.value == 0) {
                    pwr_button_down = 0;
                }
                continue;
            }
        }
        if (ev.type == EV_ABS) {
            if (ev.code == ABS_X) touch_rx = ev.value;
            else if (ev.code == ABS_Y) touch_ry = ev.value;
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            touch_down = ev.value;
        } else if (ev.type == EV_SYN) {
            if (touch_down && touch_rx > 0 && touch_ry > 0) {
                cur_raw_x = touch_rx;
                cur_raw_y = touch_ry;
                map_touch(touch_rx, touch_ry, &cur_px_x, &cur_px_y);
                if (!touch_was_down) {
                    touch_was_down = 1;
                    *out_px = cur_px_x;
                    *out_py = cur_px_y;
                    return 1;
                }
            } else if (!touch_down && touch_was_down) {
                touch_was_down = 0;
                touch_rx = -1;
                touch_ry = -1;
            }
        }
    }
    return 0;
}

/* Dashboard Tiles */
typedef struct {
    int x, y, w, h;
    const char *title;
    const char *subtitle;
    unsigned short color;
    int is_pressed;
} Tile;

static Tile tiles[4] = {
    { 16, 38, 216, 104, "CALIBRATE", "4-Point Touch Setup", COLOR_CYAN, 0 },
    { 248, 38, 216, 104, "TOOLS", "Terminal & USB SSH", COLOR_ORANGE, 0 },
    { 16, 152, 216, 104, "EXPLORER", "Filesystem & SD Card", COLOR_GREEN, 0 },
    { 248, 152, 216, 104, "BLUETOOTH", "Wireless Audio Sink", COLOR_PURPLE, 0 }
};

static void draw_top_bar(void) {
    draw_rect(0, 0, FB_WIDTH, 26, COLOR_TOPBAR);
    draw_rect(0, 26, FB_WIDTH, 2, COLOR_BAR_LINE);
    draw_string(8, 5, "TOMTOM", COLOR_WHITE, COLOR_TOPBAR);

    draw_string(68, 5, "RAW:", COLOR_GRAY, COLOR_TOPBAR);
    draw_num(100, 5, cur_raw_x, COLOR_YELLOW, COLOR_TOPBAR);
    draw_string(132, 5, ",", COLOR_GRAY, COLOR_TOPBAR);
    draw_num(140, 5, cur_raw_y, COLOR_YELLOW, COLOR_TOPBAR);

    draw_string(176, 5, "PX:", COLOR_GRAY, COLOR_TOPBAR);
    draw_num(200, 5, cur_px_x, COLOR_GREEN, COLOR_TOPBAR);
    draw_string(228, 5, ",", COLOR_GRAY, COLOR_TOPBAR);
    draw_num(236, 5, cur_px_y, COLOR_GREEN, COLOR_TOPBAR);

    draw_string(272, 5, "EV:", COLOR_GRAY, COLOR_TOPBAR);
    draw_num(296, 5, num_ev_fds, COLOR_CYAN, COLOR_TOPBAR);

    draw_string(312, 5, "K:", COLOR_GRAY, COLOR_TOPBAR);
    draw_num(328, 5, last_key_code, COLOR_PURPLE, COLOR_TOPBAR);

    draw_string(360, 5, "IP:192.168.7.2", COLOR_WHITE, COLOR_TOPBAR);
}

static void draw_tile(Tile *t) {
    unsigned short bg_color = t->is_pressed ? COLOR_GRAY : COLOR_DARKGRAY;
    draw_rect(t->x, t->y, t->w, t->h, bg_color);
    draw_rect(t->x, t->y, t->w, 6, t->color);

    int title_x = t->x + (t->w - tiny_strlen(t->title) * 8) / 2;
    draw_string(title_x, t->y + 34, t->title, COLOR_WHITE, bg_color);

    int sub_x = t->x + (t->w - tiny_strlen(t->subtitle) * 8) / 2;
    draw_string(sub_x, t->y + 64, t->subtitle, t->color, bg_color);

    for (int i = 0; i < t->w; i++) {
        draw_pixel(t->x + i, t->y, t->color);
        draw_pixel(t->x + i, t->y + t->h - 1, COLOR_DARKGRAY);
    }
    for (int j = 0; j < t->h; j++) {
        draw_pixel(t->x, t->y + j, t->color);
        draw_pixel(t->x + t->w - 1, t->y + j, COLOR_DARKGRAY);
    }
}

static void draw_launcher(void) {
    draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);
    draw_top_bar();
    for (int i = 0; i < 4; i++) {
        draw_tile(&tiles[i]);
    }
    fb_flip();
}

/* ========================================================================= */
/* APP 1: Interactive 4-Point Touchscreen Calibration                        */
/* ========================================================================= */
static void draw_crosshair(int cx, int cy, unsigned short color) {
    for (int i = -16; i <= 16; i++) {
        draw_pixel(cx + i, cy, color);
        draw_pixel(cx, cy + i, color);
    }
    for (int i = -8; i <= 8; i++) {
        draw_pixel(cx + i, cy - 8, color);
        draw_pixel(cx + i, cy + 8, color);
        draw_pixel(cx - 8, cy + i, color);
        draw_pixel(cx + 8, cy + i, color);
    }
}

static void run_calibration(void) {
    int targets_x[4] = { 40, 440, 440, 40 };
    int targets_y[4] = { 40,  40, 230, 230 };
    int samples_rx[4] = { 0 };
    int samples_ry[4] = { 0 };

    for (int step = 0; step < 4; step++) {
        draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);
        draw_rect(0, 0, FB_WIDTH, 26, COLOR_CYAN);
        draw_string(140, 5, "TOUCHSCREEN CALIBRATION", COLOR_BLACK, COLOR_CYAN);

        char msg[] = "Press Target 1 of 4 accurately with stylus";
        msg[13] = '1' + step;
        int mx = (FB_WIDTH - tiny_strlen(msg) * 8) / 2;
        draw_string(mx, 120, msg, COLOR_WHITE, COLOR_BG);

        draw_crosshair(targets_x[step], targets_y[step], COLOR_YELLOW);
        fb_flip();

        sleep_ms(300);
        reset_touch_state();

        int sum_x = 0, sum_y = 0, count = 0;
        int waiting = 1;
        struct input_event ev;
        int rx = -1, ry = -1, down = 0;

        while (waiting) {
            while (read_input_event(&ev)) {
                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_X) rx = ev.value;
                    else if (ev.code == ABS_Y) ry = ev.value;
                } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                    down = ev.value;
                } else if (ev.type == EV_SYN) {
                    if (down && rx > 0 && ry > 0) {
                        sum_x += rx;
                        sum_y += ry;
                        count++;
                    } else if (!down && count > 0) {
                        waiting = 0;
                        break;
                    }
                }
            }
            sleep_ms(20);
        }

        samples_rx[step] = int_div(sum_x, count);
        samples_ry[step] = int_div(sum_y, count);
    }

    int raw_left = (samples_rx[0] + samples_rx[3]) / 2;
    int raw_right = (samples_rx[1] + samples_rx[2]) / 2;
    int raw_top = (samples_ry[0] + samples_ry[1]) / 2;
    int raw_bottom = (samples_ry[2] + samples_ry[3]) / 2;

    int span_x = raw_right - raw_left;
    int span_y = raw_bottom - raw_top;

    calib_min_x = raw_left - int_div(span_x * 40, 400);
    calib_max_x = raw_right + int_div(span_x * 40, 400);
    calib_min_y = raw_top - int_div(span_y * 40, 190);
    calib_max_y = raw_bottom + int_div(span_y * 40, 190);

    save_calibration();

    draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);
    draw_rect(0, 0, FB_WIDTH, 26, COLOR_GREEN);
    draw_string(150, 5, "CALIBRATION SAVED!", COLOR_BLACK, COLOR_GREEN);
    draw_string(80, 80, "Calibration written to /etc/pointercal", COLOR_WHITE, COLOR_BG);
    draw_string(80, 110, "Parameters successfully applied.", COLOR_GREEN, COLOR_BG);

    draw_rect(170, 190, 140, 42, COLOR_CYAN);
    draw_string(220, 204, "FINISH", COLOR_BLACK, COLOR_CYAN);
    fb_flip();

    sleep_ms(400);
    reset_touch_state();

    int px, py;
    while (1) {
        if (poll_touch_click(&px, &py)) break;
        sleep_ms(20);
    }
    reset_touch_state();
}

/* ========================================================================= */
/* APP 2: Interactive Terminal with On-Screen Keyboard & /bin/sh             */
/* ========================================================================= */
static char *const default_envp[] = {
    "PATH=/bin:/sbin:/usr/bin:/usr/sbin",
    "HOME=/root",
    "TERM=linux",
    0
};

static char term_lines[16][64];
static int term_num_lines = 0;

static void term_add_line(const char *s) {
    if (term_num_lines < 16) {
        int len = tiny_strlen(s);
        if (len > 58) len = 58;
        tiny_memcpy(term_lines[term_num_lines], s, len);
        term_lines[term_num_lines][len] = '\0';
        term_num_lines++;
    } else {
        for (int i = 0; i < 15; i++) {
            tiny_memcpy(term_lines[i], term_lines[i + 1], 64);
        }
        int len = tiny_strlen(s);
        if (len > 58) len = 58;
        tiny_memcpy(term_lines[15], s, len);
        term_lines[15][len] = '\0';
    }
}

typedef struct {
    int x, y, w, h;
    const char *label;
    char key_char;
    const char *action;
} KeyDef;

static const KeyDef keys[] = {
    /* Row 0: y = 147, h = 22 */
    { 1,   147, 34, 22, "1", '1', 0 },
    { 36,  147, 34, 22, "2", '2', 0 },
    { 71,  147, 34, 22, "3", '3', 0 },
    { 106, 147, 34, 22, "4", '4', 0 },
    { 141, 147, 34, 22, "5", '5', 0 },
    { 176, 147, 34, 22, "6", '6', 0 },
    { 211, 147, 34, 22, "7", '7', 0 },
    { 246, 147, 34, 22, "8", '8', 0 },
    { 281, 147, 34, 22, "9", '9', 0 },
    { 316, 147, 34, 22, "0", '0', 0 },
    { 351, 147, 34, 22, "-", '-', 0 },
    { 386, 147, 34, 22, "/", '/', 0 },
    { 421, 147, 58, 22, "BKSP", '\b', 0 },

    /* Row 1: y = 171, h = 22 */
    { 1,   171, 38, 22, "q", 'q', 0 },
    { 41,  171, 38, 22, "w", 'w', 0 },
    { 81,  171, 38, 22, "e", 'e', 0 },
    { 121, 171, 38, 22, "r", 'r', 0 },
    { 161, 171, 38, 22, "t", 't', 0 },
    { 201, 171, 38, 22, "y", 'y', 0 },
    { 241, 171, 38, 22, "u", 'u', 0 },
    { 281, 171, 38, 22, "i", 'i', 0 },
    { 321, 171, 38, 22, "o", 'o', 0 },
    { 361, 171, 38, 22, "p", 'p', 0 },
    { 401, 171, 38, 22, ".", '.', 0 },
    { 441, 171, 38, 22, "~", '~', 0 },

    /* Row 2: y = 195, h = 22 */
    { 1,   195, 36, 22, "a", 'a', 0 },
    { 39,  195, 36, 22, "s", 's', 0 },
    { 77,  195, 36, 22, "d", 'd', 0 },
    { 115, 195, 36, 22, "f", 'f', 0 },
    { 153, 195, 36, 22, "g", 'g', 0 },
    { 191, 195, 36, 22, "h", 'h', 0 },
    { 229, 195, 36, 22, "j", 'j', 0 },
    { 267, 195, 36, 22, "k", 'k', 0 },
    { 305, 195, 36, 22, "l", 'l', 0 },
    { 343, 195, 36, 22, ";", ';', 0 },
    { 381, 195, 98, 22, "ENTER", '\n', 0 },

    /* Row 3: y = 219, h = 22 */
    { 1,   219, 38, 22, "z", 'z', 0 },
    { 41,  219, 38, 22, "x", 'x', 0 },
    { 81,  219, 38, 22, "c", 'c', 0 },
    { 121, 219, 38, 22, "v", 'v', 0 },
    { 161, 219, 38, 22, "b", 'b', 0 },
    { 201, 219, 38, 22, "n", 'n', 0 },
    { 241, 219, 38, 22, "m", 'm', 0 },
    { 281, 219, 38, 22, "_", '_', 0 },
    { 321, 219, 38, 22, ":", ':', 0 },
    { 361, 219, 38, 22, ">", '>', 0 },
    { 401, 219, 38, 22, "\"", '"', 0 },
    { 441, 219, 38, 22, "*", '*', 0 },

    /* Row 4: y = 243, h = 26 */
    { 1,   243, 98, 26, "SPACE", ' ', 0 },
    { 101, 243, 55, 26, "ls",   0, "ls -la" },
    { 158, 243, 50, 26, "ps",   0, "ps" },
    { 210, 243, 60, 26, "dmesg", 0, "dmesg | tail -n 6" },
    { 272, 243, 46, 26, "df",   0, "df -h" },
    { 320, 243, 75, 26, "clear", 0, "clear" },
    { 397, 243, 81, 26, "EXIT", 0, 0 }
};

#define NUM_KEYS ((int)(sizeof(keys) / sizeof(keys[0])))

static void draw_keyboard(void) {
    for (int i = 0; i < NUM_KEYS; i++) {
        const KeyDef *k = &keys[i];
        unsigned short bg = COLOR_DARKGRAY;
        unsigned short fg = COLOR_WHITE;
        if (k->key_char == '\n') { bg = COLOR_GREEN; fg = COLOR_BLACK; }
        else if (k->key_char == '\b') { bg = COLOR_BLUE; }
        else if (tiny_strcmp(k->label, "EXIT") == 0) { bg = COLOR_RED; }
        else if (k->action != 0) { bg = COLOR_PURPLE; }

        draw_rect(k->x, k->y, k->w, k->h, bg);
        for (int b = 0; b < k->w; b++) {
            draw_pixel(k->x + b, k->y, COLOR_GRAY);
            draw_pixel(k->x + b, k->y + k->h - 1, COLOR_BLACK);
        }
        for (int b = 0; b < k->h; b++) {
            draw_pixel(k->x, k->y + b, COLOR_GRAY);
            draw_pixel(k->x + k->w - 1, k->y + b, COLOR_BLACK);
        }

        int lx = k->x + (k->w - tiny_strlen(k->label) * 8) / 2;
        int ly = k->y + (k->h - 16) / 2;
        draw_string(lx, ly, k->label, fg, bg);
    }
}

static void flash_key(const KeyDef *k) {
    draw_rect(k->x, k->y, k->w, k->h, COLOR_WHITE);
    int lx = k->x + (k->w - tiny_strlen(k->label) * 8) / 2;
    int ly = k->y + (k->h - 16) / 2;
    draw_string(lx, ly, k->label, COLOR_BLACK, COLOR_WHITE);
    fb_flip();
    sleep_ms(60);

    unsigned short bg = COLOR_DARKGRAY;
    unsigned short fg = COLOR_WHITE;
    if (k->key_char == '\n') { bg = COLOR_GREEN; fg = COLOR_BLACK; }
    else if (k->key_char == '\b') { bg = COLOR_BLUE; }
    else if (tiny_strcmp(k->label, "EXIT") == 0) { bg = COLOR_RED; }
    else if (k->action != 0) { bg = COLOR_PURPLE; }

    draw_rect(k->x, k->y, k->w, k->h, bg);
    for (int b = 0; b < k->w; b++) {
        draw_pixel(k->x + b, k->y, COLOR_GRAY);
        draw_pixel(k->x + b, k->y + k->h - 1, COLOR_BLACK);
    }
    for (int b = 0; b < k->h; b++) {
        draw_pixel(k->x, k->y + b, COLOR_GRAY);
        draw_pixel(k->x + k->w - 1, k->y + b, COLOR_BLACK);
    }
    draw_string(lx, ly, k->label, fg, bg);
    fb_flip();
}

static void draw_terminal_screen(const char *input_buf) {
    draw_rect(0, 0, FB_WIDTH, 144, COLOR_BLACK);
    draw_rect(0, 0, FB_WIDTH, 22, COLOR_ORANGE);
    draw_string(8, 3, "INTERACTIVE SHELL (/bin/sh)", COLOR_BLACK, COLOR_ORANGE);

    draw_rect(340, 2, 60, 18, COLOR_DARKGRAY);
    draw_string(352, 3, "CLEAR", COLOR_WHITE, COLOR_DARKGRAY);
    draw_rect(410, 2, 65, 18, COLOR_RED);
    draw_string(425, 3, "EXIT", COLOR_WHITE, COLOR_RED);

    int start_idx = 0;
    if (term_num_lines > 6) start_idx = term_num_lines - 6;
    int y = 25;
    for (int i = start_idx; i < term_num_lines; i++) {
        draw_string(8, y, term_lines[i], COLOR_WHITE, COLOR_BLACK);
        y += 16;
    }

    draw_string(8, 124, "sh# ", COLOR_GREEN, COLOR_BLACK);
    int in_len = tiny_strlen(input_buf);
    int disp_start = 0;
    if (in_len > 50) disp_start = in_len - 50;
    draw_string(40, 124, input_buf + disp_start, COLOR_YELLOW, COLOR_BLACK);
    draw_string(40 + (in_len - disp_start) * 8, 124, "_", COLOR_WHITE, COLOR_BLACK);

    draw_rect(0, 144, FB_WIDTH, 2, COLOR_CYAN);
}

static void term_run_command(const char *cmd) {
    if (!cmd || !*cmd) return;

    if (tiny_strcmp(cmd, "clear") == 0) {
        term_num_lines = 0;
        return;
    }

    char echo_buf[64] = "sh# ";
    int ep = 4;
    const char *s = cmd;
    while (*s && ep < 58) echo_buf[ep++] = *s++;
    echo_buf[ep] = '\0';
    term_add_line(echo_buf);

    if (tiny_strcmp(cmd, "reboot") == 0) {
        term_add_line("[ERR] Reboot unsupported on this hardware.");
        term_add_line("      Please use POWER OFF.");
        return;
    }

    int pfd[2];
    if (sys_pipe(pfd) < 0) {
        term_add_line("[ERR] Cannot create pipe");
        return;
    }

    int pid = sys_fork();
    if (pid == 0) {
        sys_dup2(pfd[1], 1);
        sys_dup2(pfd[1], 2);
        sys_close(pfd[0]);
        sys_close(pfd[1]);
        char *argv[] = { "/bin/sh", "-c", (char *)cmd, 0 };
        sys_execve("/bin/sh", argv, default_envp);
        sys_exit(127);
    } else if (pid > 0) {
        sys_close(pfd[1]);
        char rbuf[256];
        char line_acc[64];
        int lp = 0;
        long n;
        while ((n = sys_read(pfd[0], rbuf, sizeof(rbuf))) > 0) {
            for (int i = 0; i < n; i++) {
                char c = rbuf[i];
                if (c == '\n' || c == '\r') {
                    if (lp > 0) {
                        line_acc[lp] = '\0';
                        term_add_line(line_acc);
                        lp = 0;
                    }
                } else if (c >= 32 && c <= 126) {
                    if (lp < 58) {
                        line_acc[lp++] = c;
                    } else {
                        line_acc[lp] = '\0';
                        term_add_line(line_acc);
                        line_acc[0] = c;
                        lp = 1;
                    }
                }
            }
            draw_terminal_screen("");
            draw_keyboard();
            fb_flip();
        }
        if (lp > 0) {
            line_acc[lp] = '\0';
            term_add_line(line_acc);
        }
        sys_close(pfd[0]);
        int status = 0;
        sys_wait4(pid, &status, 0, 0);
    }
}

static void show_terminal(void) {
    int input_len = 0;
    term_input_buf[0] = '\0';

    if (term_num_lines == 0) {
        term_add_line("Linux 6.1.182-tomtomgo-limerick (ARMv4T)");
        term_add_line("Type a command below or tap shortcuts to run.");
    }

    draw_terminal_screen(term_input_buf);
    draw_keyboard();
    fb_flip();

    sleep_ms(300);
    reset_touch_state();

    int px, py;
    int running = 1;

    while (running) {
        int ret = poll_touch_click(&px, &py);
        if (ret == 2) {
            /* Physical Power button pressed */
            show_power_menu();
            draw_terminal_screen(term_input_buf);
            draw_keyboard();
            fb_flip();
            continue;
        }
        if (ret == 1) {
            /* Header buttons */
            if (py <= 24) {
                if (px >= 335 && px <= 400) {
                    term_num_lines = 0;
                    draw_terminal_screen(term_input_buf);
                    fb_flip();
                    continue;
                }
                if (px >= 405 && px <= 478) {
                    running = 0;
                    break;
                }
            }

            int found_key = -1;
            for (int i = 0; i < NUM_KEYS; i++) {
                if (px >= keys[i].x && px < keys[i].x + keys[i].w &&
                    py >= keys[i].y && py < keys[i].y + keys[i].h) {
                    found_key = i;
                    break;
                }
            }

            if (found_key >= 0) {
                const KeyDef *k = &keys[found_key];
                flash_key(k);

                if (tiny_strcmp(k->label, "EXIT") == 0) {
                    running = 0;
                    break;
                } else if (k->action != 0) {
                    term_run_command(k->action);
                    input_len = 0;
                    term_input_buf[0] = '\0';
                } else if (k->key_char == '\n') {
                    if (input_len > 0) {
                        if (tiny_strcmp(term_input_buf, "exit") == 0) {
                            running = 0;
                            break;
                        }
                        term_run_command(term_input_buf);
                        input_len = 0;
                        term_input_buf[0] = '\0';
                    }
                } else if (k->key_char == '\b') {
                    if (input_len > 0) {
                        term_input_buf[--input_len] = '\0';
                    }
                } else if (k->key_char != '\0') {
                    if (input_len < 120) {
                        term_input_buf[input_len++] = k->key_char;
                        term_input_buf[input_len] = '\0';
                    }
                }

                draw_terminal_screen(term_input_buf);
                draw_keyboard();
                fb_flip();
            }
        }
        sleep_ms(20);
    }
    reset_touch_state();
}

/* ========================================================================= */
/* APP 3: Handheld File Explorer & Text File Viewer                          */
/* ========================================================================= */
static void ensure_sd_mounted(void) {
    sys_mkdir("/sdcard", 0755);
    sys_mount("/dev/mmcblk0p1", "/sdcard", "vfat", 0, 0);
    sys_mount("/dev/mmcblk0", "/sdcard", "vfat", 0, 0);
}

static int is_entry_dir(const char *parent, const char *name, unsigned char dtype) {
    if (dtype == 4) return 1;
    if (dtype == 8) return 0;
    char path[256];
    int lp = 0;
    while (parent[lp] && lp < 180) { path[lp] = parent[lp]; lp++; }
    if (lp > 0 && path[lp - 1] != '/') path[lp++] = '/';
    int ln = 0;
    while (name[ln] && lp < 250) { path[lp++] = name[ln++]; }
    path[lp] = '\0';

    int fd = sys_open(path, O_RDONLY | O_DIRECTORY, 0);
    if (fd >= 0) {
        sys_close(fd);
        return 1;
    }
    return 0;
}

static void show_file_viewer(const char *filepath) {
    draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BLACK);
    draw_rect(0, 0, FB_WIDTH, 24, COLOR_BLUE);
    draw_string(10, 5, filepath, COLOR_WHITE, COLOR_BLUE);

    int fd = sys_open(filepath, O_RDONLY, 0);
    if (fd >= 0) {
        long n = sys_read(fd, viewer_buf, sizeof(viewer_buf) - 1);
        sys_close(fd);
        if (n > 0) {
            viewer_buf[n] = '\0';
            int ly = 32;
            char *p = viewer_buf;
            for (int i = 0; i < 11 && *p; i++) {
                char line[60];
                int lp = 0;
                while (*p && *p != '\n' && lp < 58) line[lp++] = *p++;
                if (*p == '\n') p++;
                line[lp] = '\0';
                draw_string(10, ly, line, COLOR_WHITE, COLOR_BLACK);
                ly += 16;
            }
        } else {
            draw_string(20, 60, "(Empty file)", COLOR_GRAY, COLOR_BLACK);
        }
    } else {
        draw_string(20, 60, "Unable to read file.", COLOR_RED, COLOR_BLACK);
    }

    draw_rect(170, 235, 140, 32, COLOR_RED);
    draw_string(220, 243, "BACK", COLOR_WHITE, COLOR_RED);
    fb_flip();

    sleep_ms(300);
    reset_touch_state();

    int px, py;
    while (1) {
        int ret = poll_touch_click(&px, &py);
        if (ret == 2) {
            show_power_menu();
            break;
        }
        if (ret == 1) {
            if (px >= 140 && px <= 340 && py >= 220) break;
        }
        sleep_ms(20);
    }
    reset_touch_state();
}

static void show_file_explorer(void) {
    ensure_sd_mounted();

    explorer_current_dir[0] = '/';
    explorer_current_dir[1] = '\0';
    int num_entries = 0;
    int page = 0;

    int running = 1;
    while (running) {
        num_entries = 0;

        /* If not root, add [..] parent link */
        if (explorer_current_dir[1] != '\0') {
            tiny_memcpy(explorer_entries[num_entries], "..", 3);
            explorer_is_dir[num_entries] = 1;
            num_entries++;
        }

        int dfd = sys_open(explorer_current_dir, O_RDONLY, 0);
        if (dfd >= 0) {
            int nread = sys_getdents64(dfd, explorer_dbuf, sizeof(explorer_dbuf));
            sys_close(dfd);
            int bpos = 0;
            while (bpos < nread && num_entries < 30) {
                struct linux_dirent64 *d = (struct linux_dirent64 *)(explorer_dbuf + bpos);
                if (d->d_reclen == 0 || d->d_reclen > sizeof(explorer_dbuf)) break;
                if (tiny_strcmp(d->d_name, ".") != 0 && tiny_strcmp(d->d_name, "..") != 0) {
                    int slen = tiny_strlen(d->d_name);
                    if (slen > 60) slen = 60;
                    tiny_memcpy(explorer_entries[num_entries], d->d_name, slen);
                    explorer_entries[num_entries][slen] = '\0';
                    explorer_is_dir[num_entries] = is_entry_dir(explorer_current_dir, d->d_name, d->d_type);
                    num_entries++;
                }
                bpos += d->d_reclen;
            }
        }

        int max_page = (num_entries > 0) ? int_div(num_entries - 1, 8) : 0;
        if (page > max_page) page = max_page;
        if (page < 0) page = 0;

        /* Render File List Screen */
        draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);

        /* Top Header */
        draw_rect(0, 0, FB_WIDTH, 26, COLOR_GREEN);
        draw_string(8, 5, "EXPLORER:", COLOR_BLACK, COLOR_GREEN);
        draw_string(85, 5, explorer_current_dir, COLOR_BLACK, COLOR_GREEN);
        draw_rect(410, 2, 65, 22, COLOR_RED);
        draw_string(425, 5, "CLOSE", COLOR_WHITE, COLOR_RED);

        /* Quick Navigation Bar */
        draw_rect(0, 27, FB_WIDTH, 25, COLOR_TOPBAR);
        draw_rect(4, 29, 44, 21, COLOR_DARKGRAY);
        draw_string(18, 32, "/", COLOR_WHITE, COLOR_DARKGRAY);

        draw_rect(52, 29, 84, 21, COLOR_DARKGRAY);
        draw_string(58, 32, "/sdcard", COLOR_CYAN, COLOR_DARKGRAY);

        draw_rect(140, 29, 58, 21, COLOR_DARKGRAY);
        draw_string(148, 32, "/etc", COLOR_YELLOW, COLOR_DARKGRAY);

        draw_rect(202, 29, 58, 21, COLOR_DARKGRAY);
        draw_string(210, 32, "/bin", COLOR_GREEN, COLOR_DARKGRAY);

        draw_rect(264, 29, 58, 21, COLOR_DARKGRAY);
        draw_string(274, 32, "UP..", COLOR_WHITE, COLOR_DARKGRAY);

        draw_rect(326, 29, 80, 21, COLOR_PURPLE);
        draw_string(334, 32, "MNT SD", COLOR_WHITE, COLOR_PURPLE);

        /* File Entries (8 per page) */
        int start_entry = page * 8;
        int end_entry = start_entry + 8;
        if (end_entry > num_entries) end_entry = num_entries;

        for (int i = start_entry; i < end_entry; i++) {
            int row_idx = i - start_entry;
            int y = 54 + row_idx * 23;
            unsigned short row_bg = (row_idx % 2 == 0) ? COLOR_DARKGRAY : COLOR_BG;
            draw_rect(6, y, FB_WIDTH - 12, 21, row_bg);

            if (explorer_is_dir[i]) {
                draw_string(10, y + 3, "[DIR]", COLOR_CYAN, row_bg);
            } else {
                draw_string(10, y + 3, "[FILE]", COLOR_WHITE, row_bg);
            }
            draw_string(65, y + 3, explorer_entries[i], explorer_is_dir[i] ? COLOR_YELLOW : COLOR_WHITE, row_bg);
        }

        if (num_entries == 0) {
            draw_string(20, 80, "Empty directory or storage not mounted.", COLOR_GRAY, COLOR_BG);
        }

        /* Bottom Pagination Bar */
        draw_rect(0, 244, FB_WIDTH, 28, COLOR_TOPBAR);
        if (page > 0) {
            draw_rect(10, 247, 80, 22, COLOR_BLUE);
            draw_string(20, 250, "< PREV", COLOR_WHITE, COLOR_BLUE);
        }

        draw_string(140, 250, "Page ", COLOR_GRAY, COLOR_TOPBAR);
        draw_num(180, 250, page + 1, COLOR_WHITE, COLOR_TOPBAR);
        draw_string(195, 250, "/", COLOR_GRAY, COLOR_TOPBAR);
        draw_num(205, 250, max_page + 1, COLOR_WHITE, COLOR_TOPBAR);
        draw_string(230, 250, "(", COLOR_GRAY, COLOR_TOPBAR);
        draw_num(240, 250, num_entries, COLOR_YELLOW, COLOR_TOPBAR);
        draw_string(265, 250, " items)", COLOR_GRAY, COLOR_TOPBAR);

        if (page < max_page) {
            draw_rect(390, 247, 80, 22, COLOR_BLUE);
            draw_string(402, 250, "NEXT >", COLOR_WHITE, COLOR_BLUE);
        }

        fb_flip();
        sleep_ms(300);
        reset_touch_state();

        int px, py;
        while (1) {
            int ret = poll_touch_click(&px, &py);
            if (ret == 2) {
                show_power_menu();
                break;
            }
            if (ret == 1) {
                /* Top Close Button */
                if (px >= 405 && py <= 26) {
                    running = 0;
                    break;
                }

                /* Quick Navigation Bar (y: 27..52) */
                if (py >= 27 && py <= 52) {
                    if (px >= 4 && px <= 48) {
                        explorer_current_dir[0] = '/'; explorer_current_dir[1] = '\0';
                        page = 0;
                        break;
                    } else if (px >= 52 && px <= 136) {
                        ensure_sd_mounted();
                        tiny_memcpy(explorer_current_dir, "/sdcard", 8);
                        page = 0;
                        break;
                    } else if (px >= 140 && px <= 198) {
                        tiny_memcpy(explorer_current_dir, "/etc", 5);
                        page = 0;
                        break;
                    } else if (px >= 202 && px <= 260) {
                        tiny_memcpy(explorer_current_dir, "/bin", 5);
                        page = 0;
                        break;
                    } else if (px >= 264 && px <= 322) {
                        int len = tiny_strlen(explorer_current_dir);
                        while (len > 1 && explorer_current_dir[len - 1] != '/') len--;
                        if (len > 1) len--;
                        explorer_current_dir[len] = '\0';
                        page = 0;
                        break;
                    } else if (px >= 326 && px <= 410) {
                        ensure_sd_mounted();
                        page = 0;
                        break;
                    }
                }

                /* Bottom Pagination Buttons (y: 244..272) */
                if (py >= 244) {
                    if (px >= 10 && px <= 100 && page > 0) {
                        page--;
                        break;
                    } else if (px >= 380 && px <= 475 && page < max_page) {
                        page++;
                        break;
                    }
                }

                /* Row item clicks (y: 54..240) */
                int clicked_row = -1;
                for (int r = 0; r < 8; r++) {
                    int y = 54 + r * 23;
                    if (py >= y && py < y + 23) {
                        int item_idx = start_entry + r;
                        if (item_idx < num_entries) {
                            clicked_row = item_idx;
                        }
                        break;
                    }
                }

                if (clicked_row >= 0) {
                    if (tiny_strcmp(explorer_entries[clicked_row], "..") == 0) {
                        int len = tiny_strlen(explorer_current_dir);
                        while (len > 1 && explorer_current_dir[len - 1] != '/') len--;
                        if (len > 1) len--;
                        explorer_current_dir[len] = '\0';
                        page = 0;
                        break;
                    } else if (explorer_is_dir[clicked_row]) {
                        int len = tiny_strlen(explorer_current_dir);
                        if (explorer_current_dir[len - 1] != '/') explorer_current_dir[len++] = '/';
                        const char *s = explorer_entries[clicked_row];
                        while (*s && len < 120) explorer_current_dir[len++] = *s++;
                        explorer_current_dir[len] = '\0';
                        page = 0;
                        break;
                    } else {
                        char filepath[256];
                        int len = tiny_strlen(explorer_current_dir);
                        tiny_memcpy(filepath, explorer_current_dir, len);
                        if (filepath[len - 1] != '/') filepath[len++] = '/';
                        const char *s = explorer_entries[clicked_row];
                        while (*s && len < 250) filepath[len++] = *s++;
                        filepath[len] = '\0';

                        show_file_viewer(filepath);
                        break;
                    }
                }
            }
            sleep_ms(20);
        }
    }
    reset_touch_state();
}

/* ========================================================================= */
/* Power Management (Shutdown, Cancel)                                       */
/* ========================================================================= */
static void quiesce_bt_audio(void) {
    int pid = sys_fork();
    if (pid == 0) {
        char *argv[] = { "/usr/bin/bt_quiesce.sh", 0 };
        sys_execve("/usr/bin/bt_quiesce.sh", argv, default_envp);
        sys_exit(127);
    } else if (pid > 0) {
        int status = 0;
        sys_wait4(pid, &status, 0, 0);
    }
}

static void show_power_menu(void) {
    draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);
    draw_rect(0, 0, FB_WIDTH, 26, COLOR_RED);
    draw_string(180, 5, "POWER CONTROL", COLOR_WHITE, COLOR_RED);

    /* Reboot / USB Button */
    draw_rect(40, 36, 400, 58, COLOR_ORANGE);
    draw_string(148, 50, "REBOOT", COLOR_BLACK, COLOR_ORANGE);
    draw_string(112, 72, "Restart the TomTom without powering off", COLOR_BLACK, COLOR_ORANGE);

    /* Power Off Button */
    draw_rect(40, 104, 400, 58, COLOR_RED);
    draw_string(155, 118, "POWER OFF (SHUTDOWN)", COLOR_WHITE, COLOR_RED);
    draw_string(140, 140, "Cleanly syncs and powers off hardware", COLOR_YELLOW, COLOR_RED);

    /* Cancel Button */
    draw_rect(40, 172, 400, 58, COLOR_DARKGRAY);
    draw_string(210, 186, "CANCEL", COLOR_WHITE, COLOR_DARKGRAY);
    draw_string(170, 208, "Return to main screen", COLOR_GRAY, COLOR_DARKGRAY);

    draw_string(55, 245, "Top button toggles this menu. Reboot uses TomTom SFTRST.", COLOR_GRAY, COLOR_BG);
    fb_flip();

    sleep_ms(300);
    reset_touch_state();

    int px, py;
    while (1) {
        int ret = poll_touch_click(&px, &py);
        if (ret == 2) {
            break;
        }
        if (ret == 1) {
            /* Reboot / Connect USB */
            if (px >= 30 && px <= 450 && py >= 32 && py <= 98) {
                draw_rect(40, 36, 400, 58, COLOR_YELLOW);
                draw_string(176, 54, "REBOOTING...", COLOR_BLACK, COLOR_YELLOW);
                draw_string(104, 75, "Stopping audio/BT, then TomTom SFTRST", COLOR_BLACK, COLOR_YELLOW);
                fb_flip();
                quiesce_bt_audio();
                sys_sync();
                sleep_ms(500);
                sys_reboot(0xfee1dead, 672274793, 0x01234567, 0);
                sys_exit(0);
            }
            /* Power Off */
            if (px >= 30 && px <= 450 && py >= 100 && py <= 166) {
                draw_rect(40, 104, 400, 58, COLOR_YELLOW);
                draw_string(170, 124, "SHUTTING DOWN...", COLOR_BLACK, COLOR_YELLOW);
                fb_flip();
                quiesce_bt_audio();
                sys_sync();
                sleep_ms(500);
                sys_reboot(0xfee1dead, 672274793, 0x4321fedc, 0);
                sys_exit(0);
            }
            /* Cancel */
            if (px >= 30 && px <= 450 && py >= 168 && py <= 234) {
                break;
            }
        }
        sleep_ms(20);
    }

    pwr_button_down = 0;
    reset_touch_state();
}

/* ========================================================================= */
/* APP 4: Bluetooth Manager & Wireless Audio Sink                            */
/* ========================================================================= */
static char bt_view_buf[1024];
static char usb_view_buf[1024];

static void bt_exec_cmd(const char *cmd) {
    int pid = sys_fork();
    if (pid == 0) {
        char *argv[] = { "/bin/sh", "-c", (char *)cmd, 0 };
        sys_execve("/bin/sh", argv, default_envp);
        sys_exit(127);
    } else if (pid > 0) {
        int status = 0;
        sys_wait4(pid, &status, 0, 0);
    }
}

static void draw_usb_screen(const char *action_status) {
    draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);

    /* Top Header Bar */
    draw_rect(0, 0, FB_WIDTH, 26, COLOR_PURPLE);
    draw_string(10, 5, "USB NETWORK & SSH", COLOR_WHITE, COLOR_PURPLE);

    /* Back Button: x = 410, y = 2, w = 65, h = 22 */
    draw_rect(410, 2, 65, 22, COLOR_RED);
    draw_string(425, 5, "BACK", COLOR_WHITE, COLOR_RED);

    /* Button 1: START SSH (x: 10..210, y: 30..66) */
    draw_rect(10, 32, 200, 32, COLOR_GREEN);
    draw_string(45, 40, "START SSH", COLOR_BLACK, COLOR_GREEN);

    /* Button 2: STOP SSH (x: 220..420, y: 30..66) */
    draw_rect(220, 32, 200, 32, COLOR_RED);
    draw_string(288, 40, "STOP SSH", COLOR_WHITE, COLOR_RED);

    /* Central Display Box */
    draw_rect(10, 70, 460, 162, COLOR_BLACK);
    for (int b = 0; b < 460; b++) {
        draw_pixel(10 + b, 70, COLOR_PURPLE);
        draw_pixel(10 + b, 231, COLOR_PURPLE);
    }
    for (int b = 0; b < 162; b++) {
        draw_pixel(10, 70 + b, COLOR_PURPLE);
        draw_pixel(469, 70 + b, COLOR_PURPLE);
    }

    if (action_status && action_status[0]) {
        draw_string(20, 76, action_status, COLOR_YELLOW, COLOR_BLACK);
    }

    int fd = sys_open("/tmp/usb_out.txt", O_RDONLY, 0);
    int showed_content = 0;
    if (fd >= 0) {
        long n = sys_read(fd, usb_view_buf, sizeof(usb_view_buf) - 1);
        sys_close(fd);
        if (n > 0) {
            usb_view_buf[n] = '\0';
            int ly = 94;
            char *p = usb_view_buf;
            for (int i = 0; i < 8 && *p; i++) {
                char line[60];
                int lp = 0;
                while (*p && *p != '\n' && lp < 56) line[lp++] = *p++;
                if (*p == '\n') p++;
                line[lp] = '\0';
                draw_string(20, ly, line, COLOR_WHITE, COLOR_BLACK);
                ly += 16;
            }
            showed_content = 1;
        }
    }

    if (!showed_content) {
        draw_string(20, 96,  "Device: 192.168.7.2 on usb0", COLOR_CYAN, COLOR_BLACK);
        draw_string(20, 114, "PC: set your usb interface to 192.168.7.1", COLOR_CYAN, COLOR_BLACK);
        draw_string(20, 132, "ssh -i key root@192.168.7.2", COLOR_GREEN, COLOR_BLACK);
        draw_string(20, 150, "----------------------------------------------------", COLOR_GRAY, COLOR_BLACK);
        draw_string(20, 168, "USB + SSH start automatically at boot.", COLOR_WHITE, COLOR_BLACK);
        draw_string(20, 186, "Tap START SSH to restart them if needed.", COLOR_WHITE, COLOR_BLACK);
    }

    draw_rect(0, 238, FB_WIDTH, 34, COLOR_TOPBAR);
    draw_string(10, 246, "DEV 192.168.7.2 | PC 192.168.7.1", COLOR_GRAY, COLOR_TOPBAR);

    fb_flip();
}

static void show_usb_menu(void) {
    draw_usb_screen("Ready. USB networking starts at boot.");
    sleep_ms(300);
    reset_touch_state();

    int px, py;
    int running = 1;
    while (running) {
        int ret = poll_touch_click(&px, &py);
        if (ret == 2) {
            show_power_menu();
            draw_usb_screen("Ready.");
            reset_touch_state();
            continue;
        }
        if (ret == 1) {
            /* Back button (x: 405..480, y: 0..28) */
            if (px >= 405 && py <= 28) {
                running = 0;
                break;
            }

            /* Button 1: START SSH (x: 10..210, y: 30..66) */
            if (px >= 10 && px <= 210 && py >= 30 && py <= 66) {
                draw_rect(10, 32, 200, 32, COLOR_YELLOW);
                draw_string(45, 40, "STARTING...", COLOR_BLACK, COLOR_YELLOW);
                fb_flip();

                bt_exec_cmd("/usr/bin/usb_ssh.sh > /tmp/usb_out.txt 2>&1");
                draw_usb_screen("SSH start requested.");
                sleep_ms(200);
                reset_touch_state();
                continue;
            }

            /* Button 2: STOP SSH (x: 220..420, y: 30..66) */
            if (px >= 220 && px <= 420 && py >= 30 && py <= 66) {
                draw_rect(220, 32, 200, 32, COLOR_YELLOW);
                draw_string(276, 40, "STOPPING...", COLOR_BLACK, COLOR_YELLOW);
                fb_flip();

                bt_exec_cmd("/usr/bin/usb_stop.sh > /tmp/usb_out.txt 2>&1");
                draw_usb_screen("SSH stopped.");
                sleep_ms(200);
                reset_touch_state();
                continue;
            }
        }
        sleep_ms(20);
    }
    reset_touch_state();
}

static void draw_tools_screen(void) {
    draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);
    draw_rect(0, 0, FB_WIDTH, 26, COLOR_ORANGE);
    draw_string(10, 5, "TOOLS", COLOR_BLACK, COLOR_ORANGE);

    draw_rect(410, 2, 65, 22, COLOR_RED);
    draw_string(425, 5, "BACK", COLOR_WHITE, COLOR_RED);

    draw_rect(16, 48, 216, 150, COLOR_DARKGRAY);
    draw_rect(16, 48, 216, 6, COLOR_ORANGE);
    draw_string(84, 92, "TERMINAL", COLOR_WHITE, COLOR_DARKGRAY);
    draw_string(48, 122, "Linux Shell & Logs", COLOR_ORANGE, COLOR_DARKGRAY);

    draw_rect(248, 48, 216, 150, COLOR_DARKGRAY);
    draw_rect(248, 48, 216, 6, COLOR_PURPLE);
    draw_string(304, 92, "USB + SSH", COLOR_WHITE, COLOR_DARKGRAY);
    draw_string(280, 122, "Network & Remote", COLOR_PURPLE, COLOR_DARKGRAY);

    draw_rect(0, 238, FB_WIDTH, 34, COLOR_TOPBAR);
    draw_string(10, 246, "Terminal and USB networking grouped here", COLOR_GRAY, COLOR_TOPBAR);
    fb_flip();
}

static void show_tools_menu(void) {
    draw_tools_screen();
    sleep_ms(300);
    reset_touch_state();

    int px, py;
    int running = 1;
    while (running) {
        int ret = poll_touch_click(&px, &py);
        if (ret == 2) {
            show_power_menu();
            draw_tools_screen();
            reset_touch_state();
            continue;
        }
        if (ret == 1) {
            if (px >= 405 && py <= 28) {
                running = 0;
                break;
            }
            if (px >= 16 && px <= 232 && py >= 48 && py <= 198) {
                show_terminal();
                draw_tools_screen();
                reset_touch_state();
                continue;
            }
            if (px >= 248 && px <= 464 && py >= 48 && py <= 198) {
                show_usb_menu();
                draw_tools_screen();
                reset_touch_state();
                continue;
            }
        }
        sleep_ms(20);
    }
    reset_touch_state();
}

static void draw_bluetooth_screen(const char *action_status) {
    draw_rect(0, 0, FB_WIDTH, FB_HEIGHT, COLOR_BG);

    /* Top Header Bar */
    draw_rect(0, 0, FB_WIDTH, 26, COLOR_PURPLE);
    draw_string(10, 5, "BLUETOOTH MANAGER & AUDIO SINK", COLOR_WHITE, COLOR_PURPLE);

    /* Back Button: x = 410, y = 2, w = 65, h = 22 */
    draw_rect(410, 2, 65, 22, COLOR_RED);
    draw_string(425, 5, "BACK", COLOR_WHITE, COLOR_RED);

    /* Control Buttons: y = 32, h = 32 */
    /* Button 1: START / INIT */
    draw_rect(10, 32, 145, 32, COLOR_BLUE);
    draw_string(34, 40, "START AUDIO", COLOR_WHITE, COLOR_BLUE);

    /* Button 2: SCAN PHONES */
    draw_rect(165, 32, 145, 32, COLOR_ORANGE);
    draw_string(185, 40, "SCAN PHONES", COLOR_WHITE, COLOR_ORANGE);

    /* Button 3: SPEAKER MODE */
    draw_rect(320, 32, 150, 32, COLOR_GREEN);
    draw_string(355, 40, "STOP AUDIO", COLOR_BLACK, COLOR_GREEN);

    /* Central Display Box */
    draw_rect(10, 70, 460, 162, COLOR_BLACK);
    /* Border around central display */
    for (int b = 0; b < 460; b++) {
        draw_pixel(10 + b, 70, COLOR_PURPLE);
        draw_pixel(10 + b, 231, COLOR_PURPLE);
    }
    for (int b = 0; b < 162; b++) {
        draw_pixel(10, 70 + b, COLOR_PURPLE);
        draw_pixel(469, 70 + b, COLOR_PURPLE);
    }

    if (action_status && action_status[0]) {
        draw_string(20, 76, action_status, COLOR_YELLOW, COLOR_BLACK);
    }

    /* Read and display /tmp/bt_out.txt if present */
    int fd = sys_open("/tmp/bt_out.txt", O_RDONLY, 0);
    int showed_content = 0;
    if (fd >= 0) {
        long n = sys_read(fd, bt_view_buf, sizeof(bt_view_buf) - 1);
        sys_close(fd);
        if (n > 0) {
            bt_view_buf[n] = '\0';
            /* Count total lines in buffer */
            int total_lines = 0;
            for (char *c = bt_view_buf; *c; c++) {
                if (*c == '\n') total_lines++;
            }
            if (n > 0 && bt_view_buf[n - 1] != '\n') total_lines++;

            /* Display up to 8 latest lines */
            int max_display = 8;
            int skip_lines = (total_lines > max_display) ? (total_lines - max_display) : 0;

            char *p = bt_view_buf;
            while (skip_lines > 0 && *p) {
                if (*p++ == '\n') skip_lines--;
            }

            int ly = 94;
            for (int i = 0; i < max_display && *p; i++) {
                char line[60];
                int lp = 0;
                while (*p && *p != '\n' && lp < 56) line[lp++] = *p++;
                if (*p == '\n') p++;
                line[lp] = '\0';
                draw_string(20, ly, line, COLOR_WHITE, COLOR_BLACK);
                ly += 16;
            }
            showed_content = 1;
        }
    }

    if (!showed_content) {
        draw_string(20, 96,  "CSR BlueCore 3 @ 460800 baud (/dev/ttySAC2)", COLOR_CYAN, COLOR_BLACK);
        draw_string(20, 114, "Audio Codec: Wolfson WM8711 DAC (I2S Stereo)", COLOR_CYAN, COLOR_BLACK);
        draw_string(20, 132, "Device Name: 'TomTom ONE XL' | PIN: 0000", COLOR_GREEN, COLOR_BLACK);
        draw_string(20, 150, "----------------------------------------------------", COLOR_GRAY, COLOR_BLACK);
        draw_string(20, 168, "1. Tap [START AUDIO] to enable A2DP sink", COLOR_WHITE, COLOR_BLACK);
        draw_string(20, 186, "2. Tap [SCAN PHONES] to discover nearby phones", COLOR_WHITE, COLOR_BLACK);
        draw_string(20, 204, "3. Connect phone to 'TomTom ONE XL' and play", COLOR_YELLOW, COLOR_BLACK);
    }

    /* Bottom status: volume follows the connected phone via BlueALSA software volume. */
    draw_rect(0, 238, FB_WIDTH, 34, COLOR_TOPBAR);
    draw_string(10, 246, "A2DP | VOLUME CONTROL: PHONE", COLOR_GRAY, COLOR_TOPBAR);

    fb_flip();
}

static void show_bluetooth_menu(void) {
    draw_bluetooth_screen("Ready. Select an action above.");
    sleep_ms(300);
    reset_touch_state();

    int px, py;
    int running = 1;
    while (running) {
        int ret = poll_touch_click(&px, &py);
        if (ret == 2) {
            /* Physical Power button pressed: open power menu */
            show_power_menu();
            draw_bluetooth_screen("Ready.");
            reset_touch_state();
            continue;
        }
        if (ret == 1) {
            /* Back button (x: 405..480, y: 0..28) */
            if (px >= 405 && py <= 28) {
                running = 0;
                break;
            }

            /* Button 1: START AUDIO (x: 10..155, y: 30..66) */
            if (px >= 10 && px <= 155 && py >= 30 && py <= 66) {
                draw_rect(10, 32, 145, 32, COLOR_YELLOW);
                draw_string(30, 40, "INITIALIZING...", COLOR_BLACK, COLOR_YELLOW);
                fb_flip();

                bt_exec_cmd("/usr/bin/bt_tool.sh start > /tmp/bt_out.txt 2>&1");
                draw_bluetooth_screen("Bluetooth A2DP sink started.");
                sleep_ms(200);
                reset_touch_state();
                continue;
            }

            /* Button 2: SCAN PHONES (x: 165..310, y: 30..66) */
            if (px >= 165 && px <= 310 && py >= 30 && py <= 66) {
                draw_rect(165, 32, 145, 32, COLOR_YELLOW);
                draw_string(185, 40, "SCANNING...", COLOR_BLACK, COLOR_YELLOW);
                fb_flip();

                bt_exec_cmd("/usr/bin/bt_tool.sh scan > /tmp/bt_out.txt 2>&1");
                draw_bluetooth_screen("Bluetooth scan complete.");
                sleep_ms(200);
                reset_touch_state();
                continue;
            }

            /* Button 3: STOP AUDIO (x: 320..470, y: 30..66) */
            if (px >= 320 && px <= 470 && py >= 30 && py <= 66) {
                draw_rect(320, 32, 150, 32, COLOR_YELLOW);
                draw_string(350, 40, "STOPPING...", COLOR_BLACK, COLOR_YELLOW);
                fb_flip();

                bt_exec_cmd("/usr/bin/bt_tool.sh stop > /tmp/bt_out.txt 2>&1");
                draw_bluetooth_screen("Bluetooth audio stopped.");
                sleep_ms(200);
                reset_touch_state();
                continue;
            }
        }
        sleep_ms(20);
    }
    reset_touch_state();
}

/* ========================================================================= */
/* Entry Point & Main Application Loop                                       */
/* ========================================================================= */
void _start(void) {
    for (int retry = 0; retry < 50; retry++) {
        fb_fd = sys_open("/dev/fb0", O_RDWR, 0);
        if (fb_fd < 0) fb_fd = sys_open("/dev/fb", O_RDWR, 0);
        if (fb_fd >= 0) break;
        sleep_ms(100);
    }

    /* Disable blinking VT console cursor and enter KD_GRAPHICS */
    int tty_fd = sys_open("/dev/tty0", O_RDWR, 0);
    if (tty_fd >= 0) {
        sys_write(tty_fd, "\033[?25l", 6);
        sys_ioctl(tty_fd, 0x4B3A, 1);
        sys_close(tty_fd);
    }
    int con_fd = sys_open("/dev/console", O_RDWR, 0);
    if (con_fd >= 0) {
        sys_write(con_fd, "\033[?25l", 6);
        sys_ioctl(con_fd, 0x4B3A, 1);
        sys_close(con_fd);
    }

    for (int retry = 0; retry < 30; retry++) {
        init_input();
        if (num_ev_fds >= 2) break;
        if (retry > 10 && num_ev_fds > 0) break;
        sleep_ms(100);
    }

    load_calibration();
    draw_launcher();

    reset_touch_state();
    int px, py;
    int rescan_counter = 0;
    int boot_refresh_counter = 0;

    while (1) {
        /* Periodically redraw launcher during the first 3 seconds of boot
         * (at ~0.5s, ~1.5s, ~2.5s) to guarantee no stray console text remains */
        if (boot_refresh_counter < 150) {
            boot_refresh_counter++;
            if (boot_refresh_counter == 25 || boot_refresh_counter == 75 || boot_refresh_counter == 125) {
                draw_launcher();
            }
        }

        if (num_ev_fds < 2) {
            if (++rescan_counter >= 50) {
                rescan_counter = 0;
                init_input();
            }
        }
        if (fb_fd < 0) {
            fb_fd = sys_open("/dev/fb0", O_RDWR, 0);
            if (fb_fd >= 0) draw_launcher();
        }

        int ret = poll_touch_click(&px, &py);
        if (ret == 2) {
            /* Physical Power Button */
            show_power_menu();
            draw_launcher();
            reset_touch_state();
        } else if (ret == 1) {
            int selected_tile = -1;
            for (int i = 0; i < 4; i++) {
                if (px >= tiles[i].x && px <= tiles[i].x + tiles[i].w &&
                    py >= tiles[i].y && py <= tiles[i].y + tiles[i].h) {
                    selected_tile = i;
                    break;
                }
            }

            if (selected_tile >= 0) {
                tiles[selected_tile].is_pressed = 1;
                draw_launcher();
                sleep_ms(120);
                tiles[selected_tile].is_pressed = 0;

                switch (selected_tile) {
                case 0: /* CALIBRATE */
                    run_calibration();
                    break;
                case 1: /* TOOLS */
                    show_tools_menu();
                    break;
                case 2: /* EXPLORER */
                    show_file_explorer();
                    break;
                case 3: /* BLUETOOTH */
                    show_bluetooth_menu();
                    break;
                }

                draw_launcher();
                reset_touch_state();
            } else {
                /* Refresh top bar coordinates */
                draw_top_bar();
                fb_flip();
            }
        }
        sleep_ms(20);
    }
}
