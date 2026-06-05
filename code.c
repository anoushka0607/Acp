/*
 * ╔══════════════════════════════════════════════════════════════════╗
 * ║          2D ASCII GRAPHICS EDITOR  —  Complete Single File      ║
 * ║          Language : C  |  Library : ncurses + libm              ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  QUICK START                                                     ║
 * ║  ─────────────────────────────────────────────────────────────  ║
 * ║  1. Install ncurses dev headers (one-time):                     ║
 * ║       Ubuntu/Debian : sudo apt install libncurses-dev           ║
 * ║       Fedora/RHEL   : sudo dnf install ncurses-devel            ║
 * ║       macOS         : brew install ncurses                      ║
 * ║                                                                  ║
 * ║  2. Compile:                                                     ║
 * ║       gcc -Wall -O2 -o graphics_editor \                        ║
 * ║           graphics_editor_complete.c -lncurses -lm              ║
 * ║                                                                  ║
 * ║  3. Run:                                                         ║
 * ║       ./graphics_editor                                          ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  CONTROLS                                                        ║
 * ║  ─────────────────────────────────────────────────────────────  ║
 * ║   M / Enter   Open main menu                                    ║
 * ║   ↑ ↓ / k j   Navigate menu items                               ║
 * ║   Enter        Select / confirm                                  ║
 * ║   e            Edit selected object                              ║
 * ║   d            Delete selected object                            ║
 * ║   s            Save canvas to canvas.txt                         ║
 * ║   q / Esc      Go back / quit                                    ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  SHAPES & ALGORITHMS                                             ║
 * ║  ─────────────────────────────────────────────────────────────  ║
 * ║   Circle   — Parametric loop, 2:1 x/y aspect correction        ║
 * ║   Rectangle— 4-edge hollow outline                              ║
 * ║   Line     — Bresenham's line algorithm                         ║
 * ║   Triangle — Expanding left/right sides + filled base row       ║
 * ╠══════════════════════════════════════════════════════════════════╣
 * ║  MAKEFILE  (save as "Makefile" alongside this file to use make) ║
 * ║  ─────────────────────────────────────────────────────────────  ║
 * ║   CC     = gcc                                                   ║
 * ║   CFLAGS = -Wall -Wextra -O2                                    ║
 * ║   LIBS   = -lncurses -lm                                        ║
 * ║   TARGET = graphics_editor                                       ║
 * ║   SRC    = graphics_editor_complete.c                            ║
 * ║   all: $(TARGET)                                                 ║
 * ║   $(TARGET): $(SRC)                                              ║
 * ║       $(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)               ║
 * ║   clean:                                                         ║
 * ║       rm -f $(TARGET) canvas.txt                                 ║
 * ║   .PHONY: all clean                                              ║
 * ╚══════════════════════════════════════════════════════════════════╝
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ── Canvas dimensions ──────────────────────────────────────── */
#define COLS_W   80
#define ROWS_H   30
#define MAX_OBJ  64

/* ── Shape type constants ───────────────────────────────────── */
#define SH_CIRCLE   0
#define SH_RECT     1
#define SH_LINE     2
#define SH_TRIANGLE 3

/* ── ncurses color-pair IDs ─────────────────────────────────── */
#define CP_NORMAL   1   /* plain white text          */
#define CP_STAR     2   /* '*' character — yellow    */
#define CP_UNDER    3   /* '_' character — cyan      */
#define CP_MENU_H   4   /* highlighted menu row      */
#define CP_TITLE    5   /* window title / labels     */
#define CP_STATUS   6   /* status bar text           */
#define CP_BORDER   7   /* border lines              */
#define CP_SELECTED 8   /* selected object highlight */

/* ═══════════════════════════════════════════════════════════════
 * Object — holds every parameter for any shape type
 * ═══════════════════════════════════════════════════════════════ */
typedef struct {
    int  type;          /* SH_CIRCLE / SH_RECT / SH_LINE / SH_TRIANGLE */
    char ch;            /* drawing character: '*' or '_'                */

    /* Circle parameters */
    int  cx, cy, r;

    /* Rectangle parameters */
    int  rx, ry, rw, rh;

    /* Line parameters */
    int  x1, y1, x2, y2;

    /* Triangle parameters */
    int  tx, ty, tbase, theight;

    /* Human-readable label shown in the object list */
    char label[32];
} Object;

/* ── Global state ────────────────────────────────────────────── */
static char   canvas[ROWS_H][COLS_W + 1]; /* 2-D character grid       */
static Object objects[MAX_OBJ];           /* object list              */
static int    obj_count = 0;              /* number of live objects   */
static int    selected  = -1;             /* currently selected index */

/* ncurses window handles */
static WINDOW *win_canvas = NULL;
static WINDOW *win_status = NULL;

/* ── Forward declarations ────────────────────────────────────── */
void draw_all(void);
void render_canvas(void);
void main_menu(void);
void add_menu(void);
void objects_menu(void);
int  input_int (WINDOW *w, int y, int x, const char *prompt,
                int defval, int lo, int hi);
char input_char(WINDOW *w, int y, int x, const char *prompt);

/* ═══════════════════════════════════════════════════════════════
 * SECTION 1 — DRAWING PRIMITIVES
 *   All functions write into the global canvas[][] array.
 *   set_cell() is the single bottleneck that enforces bounds.
 * ═══════════════════════════════════════════════════════════════ */

/* Fill the canvas with spaces */
void canvas_clear(void) {
    for (int r = 0; r < ROWS_H; r++) {
        memset(canvas[r], ' ', COLS_W);
        canvas[r][COLS_W] = '\0';
    }
}

/* Safe single-cell write */
void set_cell(int r, int c, char ch) {
    if (r >= 0 && r < ROWS_H && c >= 0 && c < COLS_W)
        canvas[r][c] = ch;
}

/* ── draw_line : Bresenham's line algorithm ──────────────────── */
void draw_line(int x1, int y1, int x2, int y2, char ch) {
    int dx  = abs(x2 - x1), dy = abs(y2 - y1);
    int sx  = (x1 < x2) ? 1 : -1;
    int sy  = (y1 < y2) ? 1 : -1;
    int err = dx - dy;
    int x = x1, y = y1;
    for (;;) {
        set_cell(y, x, ch);
        if (x == x2 && y == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 <  dx) { err += dx; y += sy; }
    }
}

/* ── draw_circle : Parametric, 2:1 x/y aspect correction ────── */
/*   Monospace terminals are taller than wide, so we stretch      */
/*   the x-radius by 2 and compress y-radius by 0.5 to get a     */
/*   visually round circle.                                        */
void draw_circle(int cx, int cy, int r, char ch) {
    for (int deg = 0; deg < 360; deg++) {
        double rad = deg * M_PI / 180.0;
        int x = (int)round(cx + r * cos(rad) * 2.0);
        int y = (int)round(cy + r * sin(rad) * 0.5);
        set_cell(y, x, ch);
    }
}

/* ── draw_rect : Hollow rectangle outline ────────────────────── */
void draw_rect(int x, int y, int w, int h, char ch) {
    /* top and bottom edges */
    for (int c = x; c <= x + w; c++) {
        set_cell(y,     c, ch);
        set_cell(y + h, c, ch);
    }
    /* left and right edges */
    for (int r = y; r <= y + h; r++) {
        set_cell(r, x,     ch);
        set_cell(r, x + w, ch);
    }
}

/* ── draw_triangle : Upward-pointing hollow triangle ─────────── */
/*   Apex at (x + base/2, y).  Sides expand one column per row.  */
/*   The bottom row is fully filled to close the shape.           */
void draw_triangle(int x, int y, int base, int height, char ch) {
    int mid = base / 2;
    for (int row = 0; row < height; row++) {
        int left  = x + mid - row;
        int right = x + mid + row;
        if (row == height - 1) {
            /* filled base */
            for (int c = left; c <= right; c++)
                set_cell(y + row, c, ch);
        } else {
            set_cell(y + row, left,      ch);
            set_cell(y + row, right,     ch);
            set_cell(y,       x + mid,   ch); /* apex */
        }
    }
}

/* ── draw_all : Replay every object onto the canvas ─────────── */
void draw_all(void) {
    canvas_clear();
    for (int i = 0; i < obj_count; i++) {
        Object *o = &objects[i];
        switch (o->type) {
            case SH_CIRCLE:
                draw_circle  (o->cx, o->cy, o->r, o->ch);
                break;
            case SH_RECT:
                draw_rect    (o->rx, o->ry, o->rw, o->rh, o->ch);
                break;
            case SH_LINE:
                draw_line    (o->x1, o->y1, o->x2, o->y2, o->ch);
                break;
            case SH_TRIANGLE:
                draw_triangle(o->tx, o->ty, o->tbase, o->theight, o->ch);
                break;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 2 — NCURSES RENDERING
 * ═══════════════════════════════════════════════════════════════ */

/* Rerender the entire canvas window from the char array */
void render_canvas(void) {
    draw_all();
    werase(win_canvas);
    box(win_canvas, 0, 0);

    /* Title bar */
    wattron(win_canvas, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(win_canvas, 0, 2, " ASCII DRAW  [%d object(s)]  M=menu  s=save  q=quit ", obj_count);
    wattroff(win_canvas, COLOR_PAIR(CP_TITLE) | A_BOLD);

    /* Draw each cell with the appropriate color */
    for (int r = 0; r < ROWS_H; r++) {
        for (int c = 0; c < COLS_W; c++) {
            char ch = canvas[r][c];
            wmove(win_canvas, r + 1, c + 1);
            if (ch == '*') {
                wattron (win_canvas, COLOR_PAIR(CP_STAR) | A_BOLD);
                waddch  (win_canvas, ch);
                wattroff(win_canvas, COLOR_PAIR(CP_STAR) | A_BOLD);
            } else if (ch == '_') {
                wattron (win_canvas, COLOR_PAIR(CP_UNDER) | A_BOLD);
                waddch  (win_canvas, ch);
                wattroff(win_canvas, COLOR_PAIR(CP_UNDER) | A_BOLD);
            } else {
                wattron (win_canvas, COLOR_PAIR(CP_NORMAL));
                waddch  (win_canvas, ch);
                wattroff(win_canvas, COLOR_PAIR(CP_NORMAL));
            }
        }
    }
    wrefresh(win_canvas);
}

/* Write a one-line message to the status bar */
void status_msg(const char *msg) {
    werase(win_status);
    wattron(win_status, COLOR_PAIR(CP_STATUS));
    mvwprintw(win_status, 0, 1, " %s", msg);
    wattroff(win_status, COLOR_PAIR(CP_STATUS));
    wrefresh(win_status);
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 3 — INPUT HELPERS
 * ═══════════════════════════════════════════════════════════════ */

/* Prompt for an integer; pressing Enter with no input returns defval */
int input_int(WINDOW *w, int y, int x, const char *prompt,
              int defval, int lo, int hi) {
    char buf[16] = {0};
    mvwprintw(w, y, x, "%s [%d-%d] (def %d): ", prompt, lo, hi, defval);
    wrefresh(w);
    echo(); curs_set(1);
    wgetnstr(w, buf, (int)sizeof(buf) - 1);
    noecho(); curs_set(0);
    if (buf[0] == '\0') return defval;
    int val = atoi(buf);
    if (val < lo) val = lo;
    if (val > hi) val = hi;
    return val;
}

/* Prompt for a drawing character; anything other than '_' maps to '*' */
char input_char(WINDOW *w, int y, int x, const char *prompt) {
    char buf[8] = {0};
    mvwprintw(w, y, x, "%s [* or _] (def *): ", prompt);
    wrefresh(w);
    echo(); curs_set(1);
    wgetnstr(w, buf, (int)sizeof(buf) - 1);
    noecho(); curs_set(0);
    return (buf[0] == '_') ? '_' : '*';
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 4 — SAVE TO FILE
 * ═══════════════════════════════════════════════════════════════ */

void save_canvas(void) {
    draw_all();
    FILE *fp = fopen("canvas.txt", "w");
    if (!fp) { status_msg("ERROR: could not write canvas.txt"); return; }
    for (int r = 0; r < ROWS_H; r++) {
        /* trim trailing spaces for a clean text file */
        int last = COLS_W - 1;
        while (last > 0 && canvas[r][last] == ' ') last--;
        for (int c = 0; c <= last; c++) fputc(canvas[r][c], fp);
        fputc('\n', fp);
    }
    fclose(fp);
    status_msg("Saved to canvas.txt");
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 5 — ADD SHAPE DIALOGS
 *   Each function opens a popup window, collects parameters via
 *   input_int / input_char, appends a new Object, then closes.
 * ═══════════════════════════════════════════════════════════════ */

void add_circle(void) {
    if (obj_count >= MAX_OBJ) { status_msg("Max objects reached!"); return; }
    WINDOW *d = newwin(10, 44, 5, 10);
    box(d, 0, 0);
    wattron(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(d, 0, 2, " Add Circle ");
    wattroff(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    wrefresh(d);

    Object *o = &objects[obj_count];
    o->type = SH_CIRCLE;
    o->cx   = input_int (d, 2, 2, "Center X", 20, 0, COLS_W - 1);
    o->cy   = input_int (d, 3, 2, "Center Y", 10, 0, ROWS_H - 1);
    o->r    = input_int (d, 4, 2, "Radius  ",  6, 1, 20);
    o->ch   = input_char(d, 5, 2, "Char    ");
    snprintf(o->label, sizeof(o->label),
             "Circle   c(%d,%d) r=%d", o->cx, o->cy, o->r);
    obj_count++;
    delwin(d);
    status_msg("Circle added.");
}

void add_rect_shape(void) {
    if (obj_count >= MAX_OBJ) { status_msg("Max objects reached!"); return; }
    WINDOW *d = newwin(11, 44, 5, 10);
    box(d, 0, 0);
    wattron(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(d, 0, 2, " Add Rectangle ");
    wattroff(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    wrefresh(d);

    Object *o = &objects[obj_count];
    o->type = SH_RECT;
    o->rx   = input_int (d, 2, 2, "X      ",  5, 0, COLS_W - 2);
    o->ry   = input_int (d, 3, 2, "Y      ",  3, 0, ROWS_H - 2);
    o->rw   = input_int (d, 4, 2, "Width  ", 20, 1, COLS_W - 2);
    o->rh   = input_int (d, 5, 2, "Height ",  8, 1, ROWS_H - 2);
    o->ch   = input_char(d, 6, 2, "Char   ");
    snprintf(o->label, sizeof(o->label),
             "Rect     (%d,%d) %dx%d", o->rx, o->ry, o->rw, o->rh);
    obj_count++;
    delwin(d);
    status_msg("Rectangle added.");
}

void add_line_shape(void) {
    if (obj_count >= MAX_OBJ) { status_msg("Max objects reached!"); return; }
    WINDOW *d = newwin(11, 44, 5, 10);
    box(d, 0, 0);
    wattron(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(d, 0, 2, " Add Line ");
    wattroff(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    wrefresh(d);

    Object *o = &objects[obj_count];
    o->type = SH_LINE;
    o->x1   = input_int (d, 2, 2, "X1     ",  2, 0, COLS_W - 1);
    o->y1   = input_int (d, 3, 2, "Y1     ",  2, 0, ROWS_H - 1);
    o->x2   = input_int (d, 4, 2, "X2     ", 40, 0, COLS_W - 1);
    o->y2   = input_int (d, 5, 2, "Y2     ", 15, 0, ROWS_H - 1);
    o->ch   = input_char(d, 6, 2, "Char   ");
    snprintf(o->label, sizeof(o->label),
             "Line     (%d,%d)->(%d,%d)", o->x1, o->y1, o->x2, o->y2);
    obj_count++;
    delwin(d);
    status_msg("Line added.");
}

void add_triangle_shape(void) {
    if (obj_count >= MAX_OBJ) { status_msg("Max objects reached!"); return; }
    WINDOW *d = newwin(11, 44, 5, 10);
    box(d, 0, 0);
    wattron(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(d, 0, 2, " Add Triangle ");
    wattroff(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    wrefresh(d);

    Object *o = &objects[obj_count];
    o->type    = SH_TRIANGLE;
    o->tx      = input_int (d, 2, 2, "X (top-left)", 10, 0, COLS_W - 2);
    o->ty      = input_int (d, 3, 2, "Y (top)     ",  2, 0, ROWS_H - 2);
    o->tbase   = input_int (d, 4, 2, "Base        ", 16, 1, COLS_W / 2);
    o->theight = input_int (d, 5, 2, "Height      ", 10, 1, ROWS_H - 2);
    o->ch      = input_char(d, 6, 2, "Char        ");
    snprintf(o->label, sizeof(o->label),
             "Triangle (%d,%d) b=%d h=%d", o->tx, o->ty, o->tbase, o->theight);
    obj_count++;
    delwin(d);
    status_msg("Triangle added.");
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 6 — EDIT OBJECT (modify an existing shape's parameters)
 * ═══════════════════════════════════════════════════════════════ */

void edit_object(int idx) {
    if (idx < 0 || idx >= obj_count) return;
    Object *o = &objects[idx];
    WINDOW *d = newwin(12, 48, 5, 10);
    box(d, 0, 0);
    wattron(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(d, 0, 2, " Edit: %s ", o->label);
    wattroff(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    wrefresh(d);

    switch (o->type) {
        case SH_CIRCLE:
            o->cx = input_int (d, 2, 2, "Center X", o->cx, 0, COLS_W - 1);
            o->cy = input_int (d, 3, 2, "Center Y", o->cy, 0, ROWS_H - 1);
            o->r  = input_int (d, 4, 2, "Radius  ", o->r,  1, 20);
            o->ch = input_char(d, 5, 2, "Char    ");
            snprintf(o->label, sizeof(o->label),
                     "Circle   c(%d,%d) r=%d", o->cx, o->cy, o->r);
            break;
        case SH_RECT:
            o->rx = input_int (d, 2, 2, "X      ", o->rx, 0, COLS_W - 2);
            o->ry = input_int (d, 3, 2, "Y      ", o->ry, 0, ROWS_H - 2);
            o->rw = input_int (d, 4, 2, "Width  ", o->rw, 1, COLS_W - 2);
            o->rh = input_int (d, 5, 2, "Height ", o->rh, 1, ROWS_H - 2);
            o->ch = input_char(d, 6, 2, "Char   ");
            snprintf(o->label, sizeof(o->label),
                     "Rect     (%d,%d) %dx%d", o->rx, o->ry, o->rw, o->rh);
            break;
        case SH_LINE:
            o->x1 = input_int (d, 2, 2, "X1     ", o->x1, 0, COLS_W - 1);
            o->y1 = input_int (d, 3, 2, "Y1     ", o->y1, 0, ROWS_H - 1);
            o->x2 = input_int (d, 4, 2, "X2     ", o->x2, 0, COLS_W - 1);
            o->y2 = input_int (d, 5, 2, "Y2     ", o->y2, 0, ROWS_H - 1);
            o->ch = input_char(d, 6, 2, "Char   ");
            snprintf(o->label, sizeof(o->label),
                     "Line     (%d,%d)->(%d,%d)", o->x1, o->y1, o->x2, o->y2);
            break;
        case SH_TRIANGLE:
            o->tx      = input_int (d, 2, 2, "X (top-left)", o->tx,      0, COLS_W - 2);
            o->ty      = input_int (d, 3, 2, "Y (top)     ", o->ty,      0, ROWS_H - 2);
            o->tbase   = input_int (d, 4, 2, "Base        ", o->tbase,   1, COLS_W / 2);
            o->theight = input_int (d, 5, 2, "Height      ", o->theight, 1, ROWS_H - 2);
            o->ch      = input_char(d, 6, 2, "Char        ");
            snprintf(o->label, sizeof(o->label),
                     "Triangle (%d,%d) b=%d h=%d",
                     o->tx, o->ty, o->tbase, o->theight);
            break;
    }
    delwin(d);
    status_msg("Object updated.");
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 7 — DELETE OBJECT
 * ═══════════════════════════════════════════════════════════════ */

void delete_object(int idx) {
    if (idx < 0 || idx >= obj_count) return;
    /* Shift everything after idx one position left */
    for (int i = idx; i < obj_count - 1; i++)
        objects[i] = objects[i + 1];
    obj_count--;
    if (selected >= obj_count) selected = obj_count - 1;
    status_msg("Object deleted.");
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 8 — OBJECT LIST MENU  (browse, edit, delete)
 * ═══════════════════════════════════════════════════════════════ */

void objects_menu(void) {
    if (obj_count == 0) { status_msg("No objects yet."); return; }

    int cur = (selected >= 0 && selected < obj_count) ? selected : 0;
    int mh  = ROWS_H + 2, mw = 46;
    WINDOW *w = newwin(mh, mw, 0, COLS_W + 2);
    keypad(w, TRUE);

    for (;;) {
        werase(w);
        box(w, 0, 0);
        wattron(w, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(w, 0, 2, " Objects (%d) ", obj_count);
        wattroff(w, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(w, mh - 2, 2, "Enter/e=edit  d=delete  q=back");

        for (int i = 0; i < obj_count && i < mh - 4; i++) {
            if (i == cur) {
                wattron(w, COLOR_PAIR(CP_MENU_H) | A_BOLD);
                mvwprintw(w, 1 + i, 2, "> %-40s", objects[i].label);
                wattroff(w, COLOR_PAIR(CP_MENU_H) | A_BOLD);
            } else {
                wattron(w, COLOR_PAIR(CP_NORMAL));
                mvwprintw(w, 1 + i, 2, "  %-40s", objects[i].label);
                wattroff(w, COLOR_PAIR(CP_NORMAL));
            }
        }
        wrefresh(w);

        int ch = wgetch(w);
        if      (ch == KEY_UP   || ch == 'k') { if (cur > 0) cur--; }
        else if (ch == KEY_DOWN || ch == 'j') { if (cur < obj_count - 1) cur++; }
        else if (ch == 'e' || ch == '\n') {
            selected = cur;
            edit_object(cur);
            render_canvas();
        } else if (ch == 'd') {
            delete_object(cur);
            render_canvas();
            if (obj_count == 0) break;
            if (cur >= obj_count) cur = obj_count - 1;
        } else if (ch == 'q' || ch == 27) {
            selected = cur;
            break;
        }
    }
    delwin(w);
    touchwin(win_canvas);
    wrefresh(win_canvas);
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 9 — ADD SHAPE SUBMENU
 * ═══════════════════════════════════════════════════════════════ */

void add_menu(void) {
    const char *items[] = { "Circle", "Rectangle", "Line", "Triangle", "Back" };
    int n = 5, cur = 0;
    int mh = n + 4, mw = 24;
    WINDOW *w = newwin(mh, mw, 5, 25);
    keypad(w, TRUE);

    for (;;) {
        werase(w);
        box(w, 0, 0);
        wattron(w, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(w, 0, 2, " Add Shape ");
        wattroff(w, COLOR_PAIR(CP_TITLE) | A_BOLD);

        for (int i = 0; i < n; i++) {
            if (i == cur) {
                wattron(w, COLOR_PAIR(CP_MENU_H) | A_BOLD);
                mvwprintw(w, 1 + i, 2, "> %-18s", items[i]);
                wattroff(w, COLOR_PAIR(CP_MENU_H) | A_BOLD);
            } else {
                wattron(w, COLOR_PAIR(CP_NORMAL));
                mvwprintw(w, 1 + i, 2, "  %-18s", items[i]);
                wattroff(w, COLOR_PAIR(CP_NORMAL));
            }
        }
        wrefresh(w);

        int ch = wgetch(w);
        if      (ch == KEY_UP   || ch == 'k') { if (cur > 0) cur--; }
        else if (ch == KEY_DOWN || ch == 'j') { if (cur < n - 1) cur++; }
        else if (ch == '\n' || ch == KEY_RIGHT) {
            if (cur == 4) break;   /* "Back" */
            delwin(w);
            switch (cur) {
                case 0: add_circle();         break;
                case 1: add_rect_shape();     break;
                case 2: add_line_shape();     break;
                case 3: add_triangle_shape(); break;
            }
            render_canvas();
            w = newwin(mh, mw, 5, 25);   /* reopen menu */
            keypad(w, TRUE);
        } else if (ch == 'q' || ch == 27) break;
    }
    delwin(w);
    touchwin(win_canvas);
    wrefresh(win_canvas);
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 10 — CLEAR ALL (with confirmation dialog)
 * ═══════════════════════════════════════════════════════════════ */

void clear_all(void) {
    WINDOW *d = newwin(6, 32, 8, 24);
    box(d, 0, 0);
    wattron(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(d, 0, 2, " Confirm ");
    wattroff(d, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(d, 2, 3, "Clear all objects? (y/n)");
    wrefresh(d);
    int ch = wgetch(d);
    delwin(d);
    if (ch == 'y' || ch == 'Y') {
        obj_count = 0;
        selected  = -1;
        render_canvas();
        status_msg("Canvas cleared.");
    }
    touchwin(win_canvas);
    wrefresh(win_canvas);
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 11 — MAIN MENU
 * ═══════════════════════════════════════════════════════════════ */

void main_menu(void) {
    const char *items[] = {
        "Add Shape",
        "List / Edit / Delete Objects",
        "Save to canvas.txt",
        "Clear All",
        "Quit"
    };
    int n = 5, cur = 0;
    int mh = n + 4, mw = 36;
    WINDOW *w = newwin(mh, mw, 2, 1);
    keypad(w, TRUE);

    for (;;) {
        werase(w);
        box(w, 0, 0);
        wattron(w, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(w, 0, 2, " MAIN MENU ");
        wattroff(w, COLOR_PAIR(CP_TITLE) | A_BOLD);

        for (int i = 0; i < n; i++) {
            if (i == cur) {
                wattron(w, COLOR_PAIR(CP_MENU_H) | A_BOLD);
                mvwprintw(w, 1 + i, 2, "> %-30s", items[i]);
                wattroff(w, COLOR_PAIR(CP_MENU_H) | A_BOLD);
            } else {
                wattron(w, COLOR_PAIR(CP_NORMAL));
                mvwprintw(w, 1 + i, 2, "  %-30s", items[i]);
                wattroff(w, COLOR_PAIR(CP_NORMAL));
            }
        }
        wrefresh(w);

        int ch = wgetch(w);
        if      (ch == KEY_UP   || ch == 'k') { if (cur > 0) cur--; }
        else if (ch == KEY_DOWN || ch == 'j') { if (cur < n - 1) cur++; }
        else if (ch == '\n' || ch == KEY_RIGHT) {
            switch (cur) {
                case 0: add_menu();     break;
                case 1: objects_menu(); break;
                case 2: save_canvas();  break;
                case 3: clear_all();    break;
                case 4: delwin(w); return;
            }
        } else if (ch == 'q' || ch == 27) { delwin(w); return; }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * SECTION 12 — MAIN  (ncurses init + event loop)
 * ═══════════════════════════════════════════════════════════════ */

int main(void) {
    /* ── ncurses initialisation ─────────────────────────────── */
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Your terminal does not support color.\n");
        return 1;
    }
    start_color();
    use_default_colors();   /* -1 = transparent background */

    init_pair(CP_NORMAL,   COLOR_WHITE,  -1);
    init_pair(CP_STAR,     COLOR_YELLOW, -1);
    init_pair(CP_UNDER,    COLOR_CYAN,   -1);
    init_pair(CP_MENU_H,   COLOR_BLACK,  COLOR_YELLOW);
    init_pair(CP_TITLE,    COLOR_YELLOW, -1);
    init_pair(CP_STATUS,   COLOR_GREEN,  -1);
    init_pair(CP_BORDER,   COLOR_WHITE,  -1);
    init_pair(CP_SELECTED, COLOR_BLACK,  COLOR_GREEN);

    /* ── Create windows ──────────────────────────────────────── */
    /*   Canvas window : cols+2 wide (border), rows+2 tall       */
    /*   Status bar    : one line below canvas                    */
    int canvas_h = ROWS_H + 2;
    int canvas_w = COLS_W + 2;

    win_canvas = newwin(canvas_h, canvas_w, 0, 0);
    win_status = newwin(1, canvas_w, canvas_h, 0);

    /* ── Initial render ─────────────────────────────────────── */
    canvas_clear();
    render_canvas();
    status_msg("Press M to open menu  |  s = save  |  q = quit");

    /* ── Global key event loop ──────────────────────────────── */
    for (;;) {
        int ch = getch();
        if      (ch == 'q' || ch == 'Q')                 break;
        else if (ch == 'm' || ch == 'M' || ch == '\n') {
            main_menu();
            render_canvas();
            status_msg("Press M to open menu  |  s = save  |  q = quit");
        } else if (ch == 's') {
            save_canvas();
        }
    }

    endwin();
    printf("Goodbye!  Total objects drawn: %d\n", obj_count);
    return 0;
}
