#include "../math_app.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void press(MWApp* app, MWKey key) {
    mw_app_handle(app, key, MW_INPUT_SHORT);
}

static void hold(MWApp* app, MWKey key) {
    mw_app_handle(app, key, MW_INPUT_LONG);
}

static void tap_key(MWApp* app, const char* label) {
    assert(app->screen == MW_SCREEN_EDITOR);
    for(unsigned i = 0; i < MW_KEYBOARD_COLUMNS * MW_KEYBOARD_ROWS; ++i) {
        if(strcmp(mw_app_key_label(app, i), label) == 0) {
            /* Select a known in-range key; navigation itself is tested below. */
            app->keyboard_selected = (uint8_t)i;
            press(app, MW_KEY_OK);
            return;
        }
    }
    assert(!"Keyboard label unavailable on current page");
}

static void choose_menu(MWApp* app, const char* label) {
    const unsigned count = mw_app_menu_count(app);
    for(unsigned i = 0; i < count; ++i) {
        if(strcmp(mw_app_menu_label(app, app->selection), label) == 0) {
            press(app, MW_KEY_OK);
            return;
        }
        press(app, MW_KEY_DOWN);
    }
    assert(!"Menu item unavailable on current screen");
}

static void set_calculation(MWApp* app, const char* expression) {
    assert(app->screen == MW_SCREEN_EDITOR && app->editor == MW_EDITOR_CALCULATOR);
    assert(strlen(expression) < sizeof(app->calculation));
    strcpy(app->calculation, expression);
}

static void assert_near(float actual, float expected) {
    assert(isfinite(actual));
    assert(fabsf(actual - expected) <= 0.0001f * fmaxf(1.0f, fabsf(expected)));
}

static void test_navigation_and_exit(void) {
    MWApp app;
    mw_app_init(&app);
    assert(app.running && app.screen == MW_SCREEN_HOME && app.degrees);
    assert(mw_app_menu_count(&app) == 5);
    assert(strcmp(mw_app_menu_label(&app, 0), "Calculator") == 0);
    assert(strcmp(mw_app_menu_label(&app, 99), "") == 0);
    assert(strcmp(mw_app_key_label(&app, 99), "") == 0);
    press(&app, MW_KEY_UP);
    assert(strcmp(mw_app_menu_label(&app, app.selection), "Help & controls") == 0);
    press(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_HELP && app.help_page == 0);
    press(&app, MW_KEY_LEFT);
    assert(app.help_page == 2);
    press(&app, MW_KEY_RIGHT);
    assert(app.help_page == 0);
    press(&app, MW_KEY_DOWN);
    assert(app.help_page == 1);
    press(&app, MW_KEY_UP);
    assert(app.help_page == 0);
    press(&app, MW_KEY_BACK);
    assert(app.screen == MW_SCREEN_HOME && app.running);

    choose_menu(&app, "Recent results");
    assert(app.screen == MW_SCREEN_HISTORY && app.history_count == 0);
    press(&app, MW_KEY_OK);
    press(&app, MW_KEY_UP);
    press(&app, MW_KEY_DOWN);
    assert(app.screen == MW_SCREEN_HISTORY && app.history_count == 0);
    press(&app, MW_KEY_BACK);
    choose_menu(&app, "Examples");
    assert(app.screen == MW_SCREEN_EXAMPLES);
    press(&app, MW_KEY_BACK);
    assert(app.screen == MW_SCREEN_HOME);
    press(&app, MW_KEY_BACK);
    assert(!app.running);
    const MWApp stopped = app;
    press(&app, MW_KEY_OK);
    assert(memcmp(&app, &stopped, sizeof(app)) == 0);

    mw_app_init(&app);
    hold(&app, MW_KEY_BACK);
    assert(!app.running);
}

static void test_keyboard_and_calculation(void) {
    MWApp app;
    mw_app_init(&app);
    choose_menu(&app, "Calculator");
    assert(app.screen == MW_SCREEN_EDITOR && app.editor == MW_EDITOR_CALCULATOR);
    assert(app.keyboard_selected == 0 && app.keyboard_page == 0);
    press(&app, MW_KEY_LEFT);
    assert(strcmp(mw_app_key_label(&app, app.keyboard_selected), ")") == 0);
    press(&app, MW_KEY_RIGHT);
    assert(app.keyboard_selected == 0);
    press(&app, MW_KEY_UP);
    assert(strcmp(mw_app_key_label(&app, app.keyboard_selected), "0") == 0);
    press(&app, MW_KEY_DOWN);
    assert(app.keyboard_selected == 0);
    mw_app_handle(&app, MW_KEY_DOWN, MW_INPUT_REPEAT);
    assert(strcmp(mw_app_key_label(&app, app.keyboard_selected), "4") == 0);
    mw_app_handle(&app, MW_KEY_OK, MW_INPUT_REPEAT);
    assert(strcmp(mw_app_expression(&app), "") == 0);

    tap_key(&app, "7");
    tap_key(&app, "*");
    tap_key(&app, "8");
    assert(strcmp(mw_app_expression(&app), "7*8") == 0);
    hold(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_EDITOR && app.result_valid);
    assert_near(app.result, 56);
    assert_near(app.ans, 56);
    assert(app.history_count == 1 && app.status[0] == '\0');

    mw_app_handle(&app, MW_KEY_BACK, MW_INPUT_REPEAT);
    assert(strcmp(mw_app_expression(&app), "7*8") == 0);
    press(&app, MW_KEY_BACK);
    assert(strcmp(mw_app_expression(&app), "7*") == 0 && !app.result_valid);
    tap_key(&app, "DEL");
    assert(strcmp(mw_app_expression(&app), "7") == 0);
    press(&app, MW_KEY_BACK);
    assert(strcmp(mw_app_expression(&app), "") == 0 && app.screen == MW_SCREEN_EDITOR);
    press(&app, MW_KEY_BACK);
    assert(app.screen == MW_SCREEN_HOME && app.running);
}

static void test_error_does_not_destroy_expression_or_context(void) {
    MWApp app;
    mw_app_init(&app);
    choose_menu(&app, "Calculator");
    set_calculation(&app, "12");
    hold(&app, MW_KEY_OK);
    const char* const invalid[] = {"", "2+", "1/0", "sqrt(-1)", "10^100", "unknown(x)"};
    for(unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        set_calculation(&app, invalid[i]);
        hold(&app, MW_KEY_OK);
        assert(app.screen == MW_SCREEN_EDITOR && app.editor == MW_EDITOR_CALCULATOR);
        assert(!app.result_valid && app.status[0] != '\0');
        assert(strcmp(mw_app_expression(&app), invalid[i]) == 0);
        assert(app.history_count == 1 && app.ans == 12);
    }
    set_calculation(&app, "ans+3");
    tap_key(&app, "=");
    assert(app.result_valid && app.history_count == 2 && app.status[0] == '\0');
    assert_near(app.result, 15);
    hold(&app, MW_KEY_BACK);
    choose_menu(&app, "Graph f(x)");
    strcpy(app.function, "x+");
    hold(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_EDITOR && app.editor == MW_EDITOR_FUNCTION);
    assert(strcmp(app.function, "x+") == 0 && app.status[0]);
}

static void test_function_keys_angles_and_x_editor(void) {
    MWApp app;
    mw_app_init(&app);
    choose_menu(&app, "Calculator");
    tap_key(&app, "Fn");
    assert(app.keyboard_page == 1);
    tap_key(&app, "sin");
    assert(strcmp(mw_app_expression(&app), "sin(") == 0);
    tap_key(&app, "123");
    tap_key(&app, "9");
    tap_key(&app, "0");
    tap_key(&app, ")");
    hold(&app, MW_KEY_OK);
    assert(app.result_valid);
    assert_near(app.result, 1);
    tap_key(&app, "Fn");
    tap_key(&app, "DEG");
    assert(!app.degrees && !app.result_valid);
    assert(strcmp(mw_app_key_label(&app, 13), "RAD") == 0);
    tap_key(&app, "RAD");
    assert(app.degrees);
    tap_key(&app, "CLR");
    assert(mw_app_expression(&app)[0] == '\0' && !app.result_valid);

    tap_key(&app, "X=");
    assert(app.editor == MW_EDITOR_X && app.editor_return == MW_EDITOR_CALCULATOR);
    assert(strcmp(mw_app_expression(&app), "0") == 0);
    press(&app, MW_KEY_BACK);
    tap_key(&app, "3");
    tap_key(&app, "+");
    tap_key(&app, "4");
    hold(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_EDITOR && app.editor == MW_EDITOR_CALCULATOR);
    assert_near(app.x, 7);
    assert(app.history_count == 1 && app.ans == 1);
    assert(strstr(app.status, "x = 7") != NULL);
    tap_key(&app, "x");
    tap_key(&app, "^");
    tap_key(&app, "2");
    hold(&app, MW_KEY_OK);
    assert_near(app.result, 49);

    hold(&app, MW_KEY_BACK);
    choose_menu(&app, "Graph f(x)");
    const MWGraphWindow saved_window = app.window;
    tap_key(&app, "Fn");
    tap_key(&app, "X=");
    assert(app.editor_return == MW_EDITOR_FUNCTION);
    strcpy(app.x_input, "1/0");
    hold(&app, MW_KEY_OK);
    assert(app.editor == MW_EDITOR_X && app.status[0]);
    assert(app.x == 7);
    hold(&app, MW_KEY_BACK);
    assert(app.editor == MW_EDITOR_FUNCTION && app.screen == MW_SCREEN_EDITOR);
    assert(app.x == 7 && memcmp(&app.window, &saved_window, sizeof(saved_window)) == 0);
    tap_key(&app, "Fn");
    tap_key(&app, "X=");
    strcpy(app.x_input, "8");
    hold(&app, MW_KEY_OK);
    assert(app.editor == MW_EDITOR_FUNCTION && app.x == 8);
    tap_key(&app, "Fn");
    tap_key(&app, "X=");
    app.x_input[0] = '\0';
    press(&app, MW_KEY_BACK);
    assert(app.editor == MW_EDITOR_FUNCTION && app.x == 8);
}

static void test_history_capacity_and_context_recall(void) {
    MWApp app;
    mw_app_init(&app);
    choose_menu(&app, "Calculator");
    app.ans = 10;
    app.x = 2;
    for(unsigned i = 0; i < 10; ++i) {
        set_calculation(&app, "ans+x");
        hold(&app, MW_KEY_OK);
        assert(app.result_valid && app.history_count <= MW_HISTORY_CAPACITY);
    }
    assert(app.history_count == 8 && app.ans == 30);
    assert(app.history[0].result == 30 && app.history[0].previous_ans == 28);
    assert(app.history[7].result == 16 && app.history[7].previous_ans == 14);
    for(unsigned i = 0; i < app.history_count; ++i) {
        assert(app.history[i].result == 30 - 2 * i);
        assert(app.history[i].x == 2 && app.history[i].degrees);
    }
    app.x = 99;
    app.ans = -10;
    app.degrees = false;
    hold(&app, MW_KEY_BACK);
    choose_menu(&app, "Recent results");
    assert(app.history_selected == 0);
    press(&app, MW_KEY_UP);
    assert(app.history_selected == 7);
    press(&app, MW_KEY_DOWN);
    assert(app.history_selected == 0);
    press(&app, MW_KEY_DOWN);
    press(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_EDITOR && app.editor == MW_EDITOR_CALCULATOR);
    assert(strcmp(mw_app_expression(&app), "ans+x") == 0);
    assert(app.result_valid && app.result == 28 && app.ans == 26 && app.x == 2 && app.degrees);
    hold(&app, MW_KEY_OK);
    assert(app.result_valid && app.result == 28 && app.ans == 28);
    assert(app.history_count == 8 && app.history[0].previous_ans == 26);
}

static void test_graph_pan_zoom_trace_and_examples(void) {
    MWApp app;
    mw_app_init(&app);
    choose_menu(&app, "Examples");
    choose_menu(&app, "Sine wave");
    assert(app.screen == MW_SCREEN_GRAPH && !app.degrees && !app.trace);
    assert(strcmp(app.function, "sin(x)") == 0);
    assert(app.graph.width == MW_GRAPH_WIDTH && app.graph.height == MW_GRAPH_HEIGHT);
    assert(app.graph.valid[64]);
    const float starting_span = app.window.x_max - app.window.x_min;
    const float starting_min = app.window.x_min;
    press(&app, MW_KEY_RIGHT);
    assert_near(app.window.x_min, starting_min + 0.2f * starting_span);
    press(&app, MW_KEY_LEFT);
    assert_near(app.window.x_min, starting_min);
    const float starting_y = app.window.y_min;
    press(&app, MW_KEY_UP);
    assert(app.window.y_min > starting_y);
    press(&app, MW_KEY_DOWN);
    assert_near(app.window.y_min, starting_y);

    press(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_GRAPH_MENU);
    press(&app, MW_KEY_BACK);
    assert(app.screen == MW_SCREEN_GRAPH);
    press(&app, MW_KEY_OK);
    choose_menu(&app, "Zoom in");
    assert_near(app.window.x_max - app.window.x_min, starting_span * 2.0f / 3.0f);
    press(&app, MW_KEY_OK);
    choose_menu(&app, "Zoom out");
    assert_near(app.window.x_max - app.window.x_min, starting_span);
    press(&app, MW_KEY_OK);
    choose_menu(&app, "Trace cursor");
    assert(app.trace && app.screen == MW_SCREEN_GRAPH);
    const MWGraphWindow trace_window = app.window;
    const unsigned start_column = app.trace_column;
    press(&app, MW_KEY_RIGHT);
    assert(app.trace_column == start_column + 1);
    assert(memcmp(&app.window, &trace_window, sizeof(trace_window)) == 0);
    float x = 0;
    float y = 0;
    bool valid = false;
    mw_app_trace_value(&app, &x, &y, &valid);
    assert(valid);
    assert_near(y, sinf(x));
    for(unsigned i = 0; i < 200; ++i) press(&app, MW_KEY_LEFT);
    assert(app.trace_column == 0);
    for(unsigned i = 0; i < 200; ++i) mw_app_handle(&app, MW_KEY_RIGHT, MW_INPUT_REPEAT);
    assert(app.trace_column == MW_GRAPH_WIDTH - 1);
    press(&app, MW_KEY_UP);
    assert(app.window.x_max - app.window.x_min < starting_span);
    press(&app, MW_KEY_DOWN);
    assert_near(app.window.x_max - app.window.x_min, starting_span);
    press(&app, MW_KEY_OK);
    choose_menu(&app, "Pan graph");
    assert(!app.trace);
    press(&app, MW_KEY_OK);
    choose_menu(&app, "Reset view");
    assert(app.window.x_min == -10 && app.window.x_max == 10);
    assert(app.window.y_min == -10 && app.window.y_max == 10);
    assert(app.trace_column == MW_GRAPH_WIDTH / 2);
    press(&app, MW_KEY_OK);
    choose_menu(&app, "Edit function");
    assert(app.screen == MW_SCREEN_EDITOR && app.editor == MW_EDITOR_FUNCTION);
    assert(strcmp(app.function, "sin(x)") == 0);
    hold(&app, MW_KEY_OK);
    press(&app, MW_KEY_OK);
    choose_menu(&app, "Main menu");
    assert(app.screen == MW_SCREEN_HOME && app.running);

    const char* const names[] = {"Parabola", "Reciprocal", "Circle arc"};
    const char* const expressions[] = {"x^2-4", "1/x", "sqrt(25-x^2)"};
    for(unsigned i = 0; i < 3; ++i) {
        choose_menu(&app, "Examples");
        choose_menu(&app, names[i]);
        assert(app.screen == MW_SCREEN_GRAPH);
        assert(strcmp(app.function, expressions[i]) == 0);
        assert(mw_graph_window_valid(&app.window));
        press(&app, MW_KEY_BACK);
        assert(app.screen == MW_SCREEN_HOME);
    }
    choose_menu(&app, "Examples");
    choose_menu(&app, "18% tip");
    assert(app.screen == MW_SCREEN_EDITOR && app.result_valid);
    assert_near(app.result, 50.15f);
    hold(&app, MW_KEY_BACK);
    choose_menu(&app, "Examples");
    choose_menu(&app, "Pythagoras");
    assert(app.screen == MW_SCREEN_EDITOR && app.result_valid);
    assert_near(app.result, 5);
}

static void test_graph_ans_snapshot_and_invalid_trace(void) {
    MWApp app;
    mw_app_init(&app);
    choose_menu(&app, "Graph f(x)");
    strcpy(app.function, "ans+x");
    app.ans = 7;
    hold(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_GRAPH && app.graph_ans == 7);
    app.ans = 99;
    float x = 0;
    float y = 0;
    bool valid = false;
    mw_app_trace_value(&app, &x, &y, &valid);
    assert(valid);
    assert_near(y, 7 + x);
    press(&app, MW_KEY_BACK);
    choose_menu(&app, "Graph f(x)");
    strcpy(app.function, "sqrt(-1)");
    hold(&app, MW_KEY_OK);
    assert(app.screen == MW_SCREEN_GRAPH);
    mw_app_trace_value(&app, &x, &y, &valid);
    assert(!valid);
    for(unsigned i = 0; i < MW_GRAPH_WIDTH; ++i) {
        assert(!app.graph.valid[i] && !app.graph.connect[i]);
    }
}

static void test_text_capacity(void) {
    MWApp app;
    mw_app_init(&app);
    choose_menu(&app, "Calculator");
    memset(app.calculation, '1', MW_EXPRESSION_CAPACITY - 2);
    app.calculation[MW_EXPRESSION_CAPACITY - 2] = '\0';
    tap_key(&app, "7");
    assert(strlen(app.calculation) == MW_EXPRESSION_CAPACITY - 1);
    assert(app.calculation[MW_EXPRESSION_CAPACITY - 2] == '7');
    tap_key(&app, "8");
    assert(strlen(app.calculation) == MW_EXPRESSION_CAPACITY - 1);
    assert(app.calculation[MW_EXPRESSION_CAPACITY - 2] == '7');
    assert(strstr(app.status, "full") != NULL);
    tap_key(&app, "Fn");
    tap_key(&app, "sin");
    assert(strlen(app.calculation) == MW_EXPRESSION_CAPACITY - 1);
    assert(strstr(app.status, "full") != NULL);
    tap_key(&app, "DEL");
    assert(strlen(app.calculation) == MW_EXPRESSION_CAPACITY - 2);
    assert(app.status[0] == '\0');
    tap_key(&app, "CLR");
    assert(app.calculation[0] == '\0');
    tap_key(&app, "sqrt");
    assert(strcmp(app.calculation, "sqrt(") == 0);
    tap_key(&app, "CLR");
    tap_key(&app, "DEL");
    assert(app.calculation[0] == '\0');
}

int main(void) {
    test_navigation_and_exit();
    test_keyboard_and_calculation();
    test_error_does_not_destroy_expression_or_context();
    test_function_keys_angles_and_x_editor();
    test_history_capacity_and_context_recall();
    test_graph_pan_zoom_trace_and_examples();
    test_graph_ans_snapshot_and_invalid_trace();
    test_text_capacity();
    puts("App tests passed (editing, errors, context, history, graph, examples, bounds).");
    return 0;
}
