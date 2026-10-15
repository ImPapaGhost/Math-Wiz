#pragma once

#include "math_engine.h"
#include "math_graph.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MW_HISTORY_CAPACITY 8
#define MW_KEYBOARD_COLUMNS 6
#define MW_KEYBOARD_ROWS 4
#define MW_GRAPH_WIDTH 128
#define MW_GRAPH_HEIGHT 43

typedef enum { MW_SCREEN_HOME, MW_SCREEN_EDITOR, MW_SCREEN_GRAPH, MW_SCREEN_GRAPH_MENU,
    MW_SCREEN_HISTORY, MW_SCREEN_EXAMPLES, MW_SCREEN_HELP } MWScreen;
typedef enum { MW_EDITOR_CALCULATOR, MW_EDITOR_FUNCTION, MW_EDITOR_X } MWEditor;
typedef enum { MW_KEY_UP, MW_KEY_DOWN, MW_KEY_LEFT, MW_KEY_RIGHT, MW_KEY_OK, MW_KEY_BACK } MWKey;
typedef enum { MW_INPUT_SHORT, MW_INPUT_REPEAT, MW_INPUT_LONG } MWInput;

typedef struct {
    char expression[MW_EXPRESSION_CAPACITY];
    float result, x, previous_ans;
    bool degrees;
} MWHistoryEntry;

typedef struct {
    MWScreen screen;
    MWEditor editor, editor_return;
    uint8_t selection, keyboard_selected, keyboard_page, help_page;
    uint8_t history_count, history_selected, trace_column;
    bool degrees, running, trace, result_valid;
    float ans, x, result, graph_ans;
    char calculation[MW_EXPRESSION_CAPACITY];
    char function[MW_EXPRESSION_CAPACITY];
    char x_input[MW_EXPRESSION_CAPACITY];
    char status[40];
    MWHistoryEntry history[MW_HISTORY_CAPACITY];
    MWProgram graph_program;
    MWGraphWindow window;
    MWGraphSamples graph;
} MWApp;

void mw_app_init(MWApp* app);
void mw_app_handle(MWApp* app, MWKey key, MWInput input);
const char* mw_app_expression(const MWApp* app);
const char* mw_app_key_label(const MWApp* app, unsigned index);
const char* mw_app_menu_label(const MWApp* app, unsigned index);
unsigned mw_app_menu_count(const MWApp* app);
void mw_app_trace_value(const MWApp* app, float* x, float* y, bool* valid);
