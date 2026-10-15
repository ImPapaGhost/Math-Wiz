#include "math_app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* const main_keys[24] = {
    "7", "8", "9", "/", "(", ")", "4", "5", "6", "*", "x", "^",
    "1", "2", "3", "-", "ans", "DEL", "0", ".", "%", "+", "=", "Fn"};
static const char* const function_keys[24] = {
    "sin", "cos", "tan", "sqrt", "ln", "log", "abs", "pi", "e", "ans", "x", "neg",
    "X=", "DEG", "(", ")", "CLR", "DEL", "7", "0", ".", "^", "RUN", "123"};
static const char* const home_items[] = {
    "Calculator", "Graph f(x)", "Recent results", "Examples", "Help & controls"};
static const char* const graph_items[] = {
    "Zoom in", "Zoom out", "Trace cursor", "Edit function", "Reset view", "Main menu"};
static const char* const example_items[] = {
    "Sine wave", "Parabola", "Reciprocal", "Circle arc", "18% tip", "Pythagoras"};

static char* editable_expression(MWApp* app) {
    if(app->editor == MW_EDITOR_FUNCTION) return app->function;
    if(app->editor == MW_EDITOR_X) return app->x_input;
    return app->calculation;
}

const char* mw_app_expression(const MWApp* app) {
    if(app->editor == MW_EDITOR_FUNCTION) return app->function;
    if(app->editor == MW_EDITOR_X) return app->x_input;
    return app->calculation;
}

const char* mw_app_key_label(const MWApp* app, unsigned index) {
    if(index >= 24) return "";
    if(app->keyboard_page && index == 13) return app->degrees ? "DEG" : "RAD";
    return app->keyboard_page ? function_keys[index] : main_keys[index];
}

unsigned mw_app_menu_count(const MWApp* app) {
    if(app->screen == MW_SCREEN_HOME) return 5;
    if(app->screen == MW_SCREEN_GRAPH_MENU || app->screen == MW_SCREEN_EXAMPLES) return 6;
    return 0;
}

const char* mw_app_menu_label(const MWApp* app, unsigned index) {
    if(index >= mw_app_menu_count(app)) return "";
    if(app->screen == MW_SCREEN_HOME) return home_items[index];
    if(app->screen == MW_SCREEN_EXAMPLES) return example_items[index];
    if(index == 2 && app->trace) return "Pan graph";
    return graph_items[index];
}

static void refresh_graph(MWApp* app) {
    (void)mw_graph_sample(&app->graph_program, &app->window, MW_GRAPH_WIDTH,
        MW_GRAPH_HEIGHT, app->graph_ans, app->degrees, &app->graph);
}

static void home(MWApp* app) {
    app->screen = MW_SCREEN_HOME;
    app->selection = 0;
}

static void begin_editor(MWApp* app, MWEditor editor) {
    app->screen = MW_SCREEN_EDITOR;
    app->editor = editor;
    app->keyboard_page = 0;
    app->keyboard_selected = 0;
    app->status[0] = '\0';
    app->result_valid = false;
}

void mw_app_init(MWApp* app) {
    memset(app, 0, sizeof(*app));
    app->running = true;
    app->degrees = true;
    app->trace_column = MW_GRAPH_WIDTH / 2;
    app->window = mw_graph_default_window();
    snprintf(app->function, sizeof(app->function), "x^2-4");
    app->screen = MW_SCREEN_HOME;
}

static void append(MWApp* app, const char* text) {
    char* expression = editable_expression(app);
    size_t length = strlen(expression);
    size_t added = strlen(text);
    if(length + added >= MW_EXPRESSION_CAPACITY) {
        snprintf(app->status, sizeof(app->status), "Expression full (95)");
        return;
    }
    memcpy(expression + length, text, added + 1);
    app->status[0] = '\0';
    app->result_valid = false;
}

static void erase(MWApp* app) {
    char* expression = editable_expression(app);
    size_t length = strlen(expression);
    if(length) expression[length - 1] = '\0';
    app->status[0] = '\0';
    app->result_valid = false;
}

static void remember(MWApp* app, float result) {
    unsigned count = app->history_count;
    if(count >= MW_HISTORY_CAPACITY) count = MW_HISTORY_CAPACITY - 1;
    memmove(&app->history[1], &app->history[0], count * sizeof(app->history[0]));
    MWHistoryEntry* entry = &app->history[0];
    snprintf(entry->expression, sizeof(entry->expression), "%s", app->calculation);
    entry->result = result;
    entry->x = app->x;
    entry->previous_ans = app->ans;
    entry->degrees = app->degrees;
    if(app->history_count < MW_HISTORY_CAPACITY) app->history_count++;
    app->history_selected = 0;
}

static void run_expression(MWApp* app) {
    MWProgram program;
    size_t position = 0;
    MWStatus status = mw_compile(mw_app_expression(app), &program, &position);
    if(status != MW_OK) {
        snprintf(app->status, sizeof(app->status), "%s @%u", mw_status_message(status),
            (unsigned)(position + 1));
        app->result_valid = false;
        return;
    }
    if(app->editor == MW_EDITOR_FUNCTION) {
        app->graph_program = program;
        app->graph_ans = app->ans;
        app->screen = MW_SCREEN_GRAPH;
        app->status[0] = '\0';
        refresh_graph(app);
        return;
    }
    float result = 0;
    status = mw_evaluate(&program, app->x, app->ans, app->degrees, &result);
    if(status != MW_OK) {
        snprintf(app->status, sizeof(app->status), "%s", mw_status_message(status));
        app->result_valid = false;
        return;
    }
    if(app->editor == MW_EDITOR_X) {
        app->x = result;
        begin_editor(app, app->editor_return);
        snprintf(app->status, sizeof(app->status), "x = %.7g", (double)app->x);
        return;
    }
    remember(app, result);
    app->ans = result;
    app->result = result;
    app->result_valid = true;
    app->status[0] = '\0';
}

static void type_key(MWApp* app) {
    unsigned key = app->keyboard_selected;
    if(key == 23) {
        app->keyboard_page = !app->keyboard_page;
        return;
    }
    if(key == 22) {
        run_expression(app);
        return;
    }
    if(key == 17) {
        erase(app);
        return;
    }
    if(app->keyboard_page) {
        if(key < 7) {
            char function[8];
            snprintf(function, sizeof(function), "%s(", function_keys[key]);
            append(app, function);
        } else if(key == 11) {
            append(app, "-");
        } else if(key == 12) {
            if(app->editor != MW_EDITOR_X) {
                app->editor_return = app->editor;
                snprintf(app->x_input, sizeof(app->x_input), "%.7g", (double)app->x);
                begin_editor(app, MW_EDITOR_X);
            }
        } else if(key == 13) {
            app->degrees = !app->degrees;
            app->result_valid = false;
            snprintf(app->status, sizeof(app->status), "%s", app->degrees ? "Angles: degrees" : "Angles: radians");
        } else if(key == 16) {
            editable_expression(app)[0] = '\0';
            app->status[0] = '\0';
            app->result_valid = false;
        } else {
            append(app, function_keys[key]);
        }
    } else {
        append(app, main_keys[key]);
    }
}

static void open_example(MWApp* app) {
    unsigned selection = app->selection;
    static const char* const functions[] = {"sin(x)", "x^2-4", "1/x", "sqrt(25-x^2)"};
    if(selection < 4) {
        begin_editor(app, MW_EDITOR_FUNCTION);
        snprintf(app->function, sizeof(app->function), "%s", functions[selection]);
        app->trace = false;
        app->trace_column = MW_GRAPH_WIDTH / 2;
        app->window = mw_graph_default_window();
        if(selection == 0) {
            app->degrees = false;
            app->window = (MWGraphWindow){-6.2831853f, 6.2831853f, -1.5f, 1.5f};
        } else if(selection == 1) {
            app->window = (MWGraphWindow){-6, 6, -6, 6};
        } else if(selection == 2) {
            app->window = (MWGraphWindow){-5, 5, -5, 5};
        } else {
            app->window = (MWGraphWindow){-6, 6, -1, 6};
        }
        run_expression(app);
    } else {
        begin_editor(app, MW_EDITOR_CALCULATOR);
        snprintf(app->calculation, sizeof(app->calculation), "%s",
            selection == 4 ? "42.50*(1+18%)" : "sqrt(3^2+4^2)");
        run_expression(app);
    }
}

static void select_menu(MWApp* app) {
    if(app->screen == MW_SCREEN_HOME) {
        switch(app->selection) {
        case 0: begin_editor(app, MW_EDITOR_CALCULATOR); break;
        case 1: begin_editor(app, MW_EDITOR_FUNCTION); break;
        case 2: app->screen = MW_SCREEN_HISTORY; app->history_selected = 0; break;
        case 3: app->screen = MW_SCREEN_EXAMPLES; app->selection = 0; break;
        case 4: app->screen = MW_SCREEN_HELP; app->help_page = 0; break;
        default: break;
        }
    } else if(app->screen == MW_SCREEN_EXAMPLES) {
        open_example(app);
    } else {
        unsigned selection = app->selection;
        app->screen = MW_SCREEN_GRAPH;
        switch(selection) {
        case 0: (void)mw_graph_zoom(&app->window, 2.0f / 3.0f); break;
        case 1: (void)mw_graph_zoom(&app->window, 1.5f); break;
        case 2: app->trace = !app->trace; break;
        case 3: begin_editor(app, MW_EDITOR_FUNCTION); return;
        case 4:
            app->window = mw_graph_default_window();
            app->trace_column = MW_GRAPH_WIDTH / 2;
            break;
        case 5: home(app); return;
        default: break;
        }
        refresh_graph(app);
    }
}

static void graph_input(MWApp* app, MWKey key) {
    if(key == MW_KEY_OK) {
        app->screen = MW_SCREEN_GRAPH_MENU;
        app->selection = 0;
        return;
    }
    if(key == MW_KEY_BACK) {
        home(app);
        return;
    }
    if(app->trace) {
        if(key == MW_KEY_LEFT && app->trace_column > 0) app->trace_column--;
        if(key == MW_KEY_RIGHT && app->trace_column < MW_GRAPH_WIDTH - 1) app->trace_column++;
        if(key == MW_KEY_UP) (void)mw_graph_zoom(&app->window, 2.0f / 3.0f);
        if(key == MW_KEY_DOWN) (void)mw_graph_zoom(&app->window, 1.5f);
    } else {
        if(key == MW_KEY_LEFT) (void)mw_graph_pan(&app->window, -0.2f, 0);
        if(key == MW_KEY_RIGHT) (void)mw_graph_pan(&app->window, 0.2f, 0);
        if(key == MW_KEY_UP) (void)mw_graph_pan(&app->window, 0, 0.2f);
        if(key == MW_KEY_DOWN) (void)mw_graph_pan(&app->window, 0, -0.2f);
    }
    refresh_graph(app);
}

void mw_app_trace_value(const MWApp* app, float* x, float* y, bool* valid) {
    *x = mw_graph_x(&app->window, app->trace_column, MW_GRAPH_WIDTH);
    *valid = mw_evaluate(&app->graph_program, *x, app->graph_ans, app->degrees, y) == MW_OK;
}

void mw_app_handle(MWApp* app, MWKey key, MWInput input) {
    if(!app->running) return;
    if(input == MW_INPUT_LONG) {
        if(key == MW_KEY_BACK) {
            if(app->screen == MW_SCREEN_HOME) app->running = false;
            else if(app->screen == MW_SCREEN_EDITOR && app->editor == MW_EDITOR_X)
                begin_editor(app, app->editor_return);
            else home(app);
        } else if(key == MW_KEY_OK && app->screen == MW_SCREEN_EDITOR) {
            run_expression(app);
        }
        return;
    }
    if(input == MW_INPUT_REPEAT && (key == MW_KEY_OK || key == MW_KEY_BACK)) return;
    if(app->screen == MW_SCREEN_EDITOR) {
        unsigned selected = app->keyboard_selected;
        if(key == MW_KEY_UP) selected = (selected + 18) % 24;
        if(key == MW_KEY_DOWN) selected = (selected + 6) % 24;
        if(key == MW_KEY_LEFT) selected = (selected / 6) * 6 + (selected + 5) % 6;
        if(key == MW_KEY_RIGHT) selected = (selected / 6) * 6 + (selected + 1) % 6;
        app->keyboard_selected = (uint8_t)selected;
        if(key == MW_KEY_OK) type_key(app);
        if(key == MW_KEY_BACK) {
            if(mw_app_expression(app)[0]) erase(app);
            else if(app->editor == MW_EDITOR_X) begin_editor(app, app->editor_return);
            else home(app);
        }
        return;
    }
    if(app->screen == MW_SCREEN_GRAPH) {
        graph_input(app, key);
        return;
    }
    if(app->screen == MW_SCREEN_HISTORY) {
        if(key == MW_KEY_BACK) home(app);
        else if(app->history_count) {
            if(key == MW_KEY_UP) app->history_selected = (app->history_selected + app->history_count - 1) % app->history_count;
            if(key == MW_KEY_DOWN) app->history_selected = (app->history_selected + 1) % app->history_count;
            if(key == MW_KEY_OK) {
                const MWHistoryEntry* entry = &app->history[app->history_selected];
                begin_editor(app, MW_EDITOR_CALCULATOR);
                snprintf(app->calculation, sizeof(app->calculation), "%s", entry->expression);
                app->x = entry->x;
                app->ans = entry->previous_ans;
                app->degrees = entry->degrees;
                app->result = entry->result;
                app->result_valid = true;
            }
        }
        return;
    }
    if(app->screen == MW_SCREEN_HELP) {
        if(key == MW_KEY_BACK || key == MW_KEY_OK) home(app);
        if(key == MW_KEY_RIGHT || key == MW_KEY_DOWN) app->help_page = (app->help_page + 1) % 3;
        if(key == MW_KEY_LEFT || key == MW_KEY_UP) app->help_page = (app->help_page + 2) % 3;
        return;
    }
    if(key == MW_KEY_BACK) {
        if(app->screen == MW_SCREEN_HOME) app->running = false;
        else if(app->screen == MW_SCREEN_GRAPH_MENU) app->screen = MW_SCREEN_GRAPH;
        else home(app);
        return;
    }
    unsigned count = mw_app_menu_count(app);
    if(!count) return;
    if(key == MW_KEY_UP) app->selection = (app->selection + count - 1) % count;
    if(key == MW_KEY_DOWN) app->selection = (app->selection + 1) % count;
    if(key == MW_KEY_OK) select_menu(app);
}
