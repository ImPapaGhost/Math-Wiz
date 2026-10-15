#include "math_ui.h"

#include <stdio.h>
#include <string.h>

/* Public-domain Misc Fixed 4x6 font, distributed by u8g2.
 * Original notice: "Public domain font. Share and enjoy."
 * Source: flipperdevices/flipperzero-firmware, tag 1.4.3,
 * lib/u8g2/u8g2_fonts.c, u8g2_font_4x6_tr (unmodified glyph data).
 * Six-pixel maximum ink height keeps descenders inside our 8px rows.
 */
static const uint8_t mw_font_compact[734] =
    "_\0\2\2\3\3\2\4\4\4\6\0\377\5\377\5\377\0\356\1\334\2\301 \5\200\315\0!\6\351\310"
    "\254\0\42\6\223\313$\25#\12\254\310\244\64T\32*\1$\11\263\307\245\241\301H\11%\10\253\310d"
    "\324F\1&\11\254\310\305\24\253\230\2'\5\321\313\10(\7\362\307\251f\0)\10\262\307\304T)\0"
    "*\7\253\310\244j\65+\10\253\310\305\264b\2,\6\222\307)\0-\5\213\312\14.\5\311\310\4/"
    "\7\253\310Ve\4\60\10\253\310UCU\0\61\7\253\310%Y\15\62\7\253\310\65S\32\63\10\253\310"
    "\314\224\301\2\64\10\253\310$\65b\1\65\10\253\310\214\250\301\2\66\7\253\310M\325\2\67\10\253\310\314"
    "TF\0\70\7\253\310\255\326\2\71\7\253\310\265\344\2:\6\341\310\304\0;\7\252\307e\250\0<\10"
    "\253\310\246\32d\20=\6\233\311l\60>\11\253\310d\220A*\1\77\11\253\310h\220\62L\0@\7"
    "\253\310-\33\10A\10\253\310UC\251\0B\10\253\310\250\264\322\2C\10\253\310U\62U\0D\10\253"
    "\310\250d-\0E\10\253\310\214\250\342\0F\10\253\310\214\250b\4G\10\253\310\315\244\222\0H\10\253"
    "\310$\65\224\12I\7\253\310\254X\15J\7\253\310\226\252\2K\10\253\310$\265\222\12L\7\253\310\304"
    "\346\0M\10\253\310\244\61\224\12N\10\253\310\252\241$\0O\7\253\310UV\5P\10\253\310\250\264b"
    "\4Q\10\263\307UV\15\2R\10\253\310\250\264\222\12S\10\253\310m\220\301\2T\7\253\310\254\330\2"
    "U\7\253\310$\327\10V\10\253\310$k\244\4W\10\253\310$\65\206\12X\10\253\310$\325R\1Y"
    "\10\253\310$UV\0Z\7\253\310\314T\16[\6\352\310\254J\134\11\253\310\304\14\62\210\1]\6\252"
    "\310\250j^\5\223\313\65_\5\213\307\14`\6\322\313\304\0a\7\243\310-\225\4b\10\253\310D\225"
    "\324\2c\7\243\310\315\14\4d\10\253\310\246\245\222\0e\6\243\310USf\10\253\310\246\264b\2g"
    "\10\253\307\255$\27\0h\10\253\310D\225\254\0i\10\253\310e$\323\0j\10\263\307fX.\0k"
    "\10\253\310\304\264\222\12l\7\253\310\310\326\0m\10\243\310\244\241T\0n\7\243\310\250d\5o\7\243"
    "\310U\252\2p\10\253\307\250\264b\4q\10\253\307-\225d\0r\10\243\310\244\25#\0s\10\243\310"
    "\215\14\26\0t\10\253\310\245\25\63\10u\7\243\310$+\11v\7\243\310$\253\2w\10\243\310$\65"
    "T\0x\7\243\310\244\62\25y\10\253\307$\225\344\2z\7\243\310\314\224\6{\10\263\307\246$k\20"
    "|\6\351\310\14\1}\11\263\307d\20UL\21~\7\224\313%\225\0\0\0\0\4\377\377\0";

static void compact_font(Canvas* canvas) {
    canvas_set_custom_u8g2_font(canvas, mw_font_compact);
}

/* Measure in the active font, so even long numbers stay inside the LCD. */
static void text_fit(Canvas* canvas, int x, int y, unsigned width, const char* text) {
    char buffer[MW_EXPRESSION_CAPACITY];
    snprintf(buffer, sizeof(buffer), "%s", text);
    size_t length = strlen(buffer);
    bool shortened = false;
    while(length && canvas_string_width(canvas, buffer) > width) {
        buffer[--length] = '\0';
        shortened = true;
    }
    if(shortened && length) {
        buffer[length - 1] = '~';
        while(length > 1 && canvas_string_width(canvas, buffer) > width) {
            buffer[--length] = '\0';
            buffer[length - 1] = '~';
        }
    }
    canvas_draw_str(canvas, x, y, buffer);
}

static void header(Canvas* canvas, const char* title, const char* right) {
    canvas_set_font(canvas, FontSecondary);
    text_fit(canvas, 2, 8, 90, title);
    compact_font(canvas);
    canvas_draw_str_aligned(canvas, 126, 8, AlignRight, AlignBottom, right);
    canvas_draw_line(canvas, 0, 10, 127, 10);
}

static void footer(Canvas* canvas, const char* text) {
    compact_font(canvas);
    canvas_draw_line(canvas, 0, 55, 127, 55);
    text_fit(canvas, 2, 63, 124, text);
}

static void draw_menu(Canvas* canvas, const MWApp* app) {
    unsigned count = mw_app_menu_count(app);
    unsigned selected = app->selection < count ? app->selection : 0;
    unsigned first = selected < 4 ? 0 : selected - 3;
    const char* title = app->screen == MW_SCREEN_HOME ? "MATH-WIZ" :
                        app->screen == MW_SCREEN_EXAMPLES ? "TRY A FUNCTION" : "GRAPH TOOLS";
    char position[12];
    snprintf(position, sizeof(position), "%u/%u", selected + 1, count);
    header(canvas, title, position);
    canvas_set_font(canvas, FontSecondary);
    for(unsigned row = 0; row < 4 && first + row < count; ++row) {
        bool active = first + row == selected;
        int top = 13 + row * 10;
        if(active) canvas_draw_box(canvas, 1, top, 123, 10);
        canvas_set_color(canvas, active ? ColorWhite : ColorBlack);
        text_fit(canvas, 4, top + 8, 117, mw_app_menu_label(app, first + row));
        canvas_set_color(canvas, ColorBlack);
    }
    if(count > 4) {
        canvas_draw_line(canvas, 127, 13, 127, 52);
        unsigned thumb = 40 * 4 / count;
        unsigned offset = first * (40 - thumb) / (count - 4);
        canvas_draw_box(canvas, 126, 13 + offset, 2, thumb);
    }
    footer(canvas, app->screen == MW_SCREEN_HOME ? "OK open       Back exit" :
                                                   "OK choose     Back return");
}

static void draw_editor(Canvas* canvas, const MWApp* app) {
    const char* title = app->editor == MW_EDITOR_FUNCTION ? "F(X)" :
                        app->editor == MW_EDITOR_X ? "SET X" : "CALC";
    compact_font(canvas);
    canvas_draw_str(canvas, 2, 7, title);
    canvas_draw_str_aligned(canvas, 126, 7, AlignRight, AlignBottom,
                           app->degrees ? "DEG" : "RAD");
    const char* expression = mw_app_expression(app);
    canvas_set_font(canvas, FontSecondary);
    bool cropped = false;
    while(*expression && canvas_string_width(canvas, expression) > 116) {
        ++expression;
        cropped = true;
    }
    canvas_draw_str(canvas, 2, 18, cropped ? "<" : ">");
    canvas_draw_str(canvas, 10, 18, *expression ? expression : "_");
    compact_font(canvas);
    char result[40];
    if(app->status[0]) {
        text_fit(canvas, 2, 27, 124, app->status);
    } else if(app->result_valid) {
        snprintf(result, sizeof(result), "= %.7g", (double)app->result);
        text_fit(canvas, 2, 27, 124, result);
    } else {
        text_fit(canvas, 2, 27, 124,
                 app->editor == MW_EDITOR_FUNCTION ? "Hold OK: plot  Back: del" :
                                                      "Hold OK: =     Back: del");
    }
    canvas_draw_line(canvas, 0, 29, 127, 29);
    for(unsigned key = 0; key < MW_KEYBOARD_COLUMNS * MW_KEYBOARD_ROWS; ++key) {
        int left = (key % MW_KEYBOARD_COLUMNS) * 21;
        int top = 31 + (key / MW_KEYBOARD_COLUMNS) * 8;
        bool active = key == app->keyboard_selected;
        if(active) canvas_draw_box(canvas, left, top, 21, 8);
        canvas_set_color(canvas, active ? ColorWhite : ColorBlack);
        const char* label = mw_app_key_label(app, key);
        unsigned label_width = canvas_string_width(canvas, label);
        if(label_width <= 20) {
            canvas_draw_str(canvas, left + (21 - label_width) / 2, top + 6, label);
        } else {
            text_fit(canvas, left + 1, top + 6, 19, label);
        }
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_graph(Canvas* canvas, const MWApp* app) {
    char line[MW_EXPRESSION_CAPACITY + 8];
    snprintf(line, sizeof(line), "y=%s", app->function);
    header(canvas, line, app->degrees ? "DEG" : "RAD");
    int16_t px, py;
    if(mw_graph_project(&app->window, 0, app->window.y_min,
                        MW_GRAPH_WIDTH, MW_GRAPH_HEIGHT, &px, &py)) {
        for(int y = 11; y < 53; y += 3) canvas_draw_dot(canvas, px, y);
    }
    if(mw_graph_project(&app->window, app->window.x_min, 0,
                        MW_GRAPH_WIDTH, MW_GRAPH_HEIGHT, &px, &py)) {
        for(int x = 0; x < 128; x += 3) canvas_draw_dot(canvas, x, py + 10);
    }
    for(unsigned x = 0; x < MW_GRAPH_WIDTH; ++x) {
        if(!app->graph.valid[x]) continue;
        int y = app->graph.y[x] + 10;
        if(x && app->graph.connect[x] && app->graph.valid[x - 1]) {
            canvas_draw_line(canvas, x - 1, app->graph.y[x - 1] + 10, x, y);
        } else {
            canvas_draw_dot(canvas, x, y);
        }
    }
    if(app->trace) {
        unsigned column = app->trace_column < MW_GRAPH_WIDTH ? app->trace_column : MW_GRAPH_WIDTH - 1;
        for(int y = 11; y < 53; y += 4) canvas_draw_dot(canvas, column, y);
        if(column < MW_GRAPH_WIDTH && app->graph.valid[column]) {
            int y = app->graph.y[column] + 10;
            int left = column > 2 ? column - 2 : 0;
            int right = column < 125 ? column + 2 : 127;
            canvas_draw_line(canvas, left, y, right, y);
            canvas_draw_line(canvas, column, y > 12 ? y - 2 : 10,
                             column, y < 51 ? y + 2 : 52);
        }
        float x, y;
        bool valid;
        mw_app_trace_value(app, &x, &y, &valid);
        if(valid) snprintf(line, sizeof(line), "x=%.3g y=%.3g", (double)x, (double)y);
        else snprintf(line, sizeof(line), "x=%.3g y=undefined", (double)x);
    } else {
        snprintf(line, sizeof(line), "x[%.3g,%.3g] OK tools",
                 (double)app->window.x_min, (double)app->window.x_max);
    }
    footer(canvas, line);
}

static void draw_history(Canvas* canvas, const MWApp* app) {
    header(canvas, "HISTORY", "SESSION");
    compact_font(canvas);
    if(!app->history_count) {
        text_fit(canvas, 5, 29, 118, "No calculations yet.");
        text_fit(canvas, 5, 41, 118, "Run a calculation first.");
    } else {
        unsigned first = app->history_selected < 3 ? 0 : app->history_selected - 2;
        for(unsigned row = 0; row < 3 && first + row < app->history_count; ++row) {
            const MWHistoryEntry* entry = &app->history[first + row];
            bool active = first + row == app->history_selected;
            int top = 12 + row * 14;
            if(active) canvas_draw_box(canvas, 1, top, 126, 14);
            canvas_set_color(canvas, active ? ColorWhite : ColorBlack);
            text_fit(canvas, 4, top + 6, 119, entry->expression);
            char result[32];
            snprintf(result, sizeof(result), "= %.7g", (double)entry->result);
            text_fit(canvas, 10, top + 13, 112, result);
            canvas_set_color(canvas, ColorBlack);
        }
    }
    footer(canvas, app->history_count ? "OK reuse      Back menu" : "Back menu");
}

static void draw_help(Canvas* canvas, const MWApp* app) {
    static const char* const pages[3][5] = {
        {"Arrows: choose a key", "OK: type   Hold OK: =", "Back: delete",
         "Hold Back: menu", "Fn: funcs, angle, CLR, X"},
        {"Arrows: pan the graph", "OK: graph tools", "Trace: Left/Right move",
         "Trace: Up/Down zoom", "Back: main menu"},
        {"Use * for multiplication", "^ power    % divide 100", "DEG/RAD changes trig",
         "Ans: last result", "History: this session"},
    };
    unsigned page = app->help_page < 3 ? app->help_page : 0;
    char position[12];
    snprintf(position, sizeof(position), "%u/3", page + 1);
    header(canvas, page == 0 ? "CALCULATOR" : page == 1 ? "GRAPHING" : "MATH NOTES", position);
    compact_font(canvas);
    for(unsigned row = 0; row < 5; ++row) text_fit(canvas, 2, 20 + row * 8, 124, pages[page][row]);
    footer(canvas, "Left/Right page  Back menu");
}

void mw_draw(Canvas* canvas, const MWApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    switch(app->screen) {
    case MW_SCREEN_EDITOR: draw_editor(canvas, app); break;
    case MW_SCREEN_GRAPH: draw_graph(canvas, app); break;
    case MW_SCREEN_HISTORY: draw_history(canvas, app); break;
    case MW_SCREEN_HELP: draw_help(canvas, app); break;
    default: draw_menu(canvas, app); break;
    }
    canvas_set_color(canvas, ColorBlack);
}
