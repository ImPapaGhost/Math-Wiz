#include "../math_graph.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static MWGraphSamples sample(
    const char* expression,
    MWGraphWindow window,
    uint16_t width,
    uint16_t height,
    float ans,
    bool degrees) {
    MWProgram program;
    MWGraphSamples output;
    assert(mw_compile(expression, &program, NULL) == MW_OK);
    assert(mw_graph_sample(&program, &window, width, height, ans, degrees, &output));
    return output;
}

static void test_coordinate_mapping(void) {
    const MWGraphWindow window = mw_graph_default_window();
    int16_t x = -1;
    int16_t y = -1;
    assert(mw_graph_window_valid(&window));
    assert(mw_graph_project(&window, -10, 10, 128, 43, &x, &y));
    assert(x == 0 && y == 0);
    assert(mw_graph_project(&window, 10, -10, 128, 43, &x, &y));
    assert(x == 127 && y == 42);
    assert(mw_graph_project(&window, 0, 0, 128, 43, &x, &y));
    assert(x == 64 && y == 21);
    assert(!mw_graph_project(&window, 11, 0, 128, 43, &x, &y));
    assert(x == 64 && y == 21);
    assert(!mw_graph_project(&window, NAN, 0, 128, 43, &x, &y));
    assert(!mw_graph_project(&window, 0, INFINITY, 128, 43, &x, &y));
    assert(!mw_graph_project(&window, 0, 0, 1, 43, &x, &y));
    assert(mw_graph_x(&window, 0, 128) == -10);
    assert(mw_graph_x(&window, 127, 128) == 10);
    assert(mw_graph_x(&window, 64, 127) > 0);
    assert(isnan(mw_graph_x(&window, 128, 128)));
    assert(isnan(mw_graph_x(&window, 0, 1)));
}

static void test_constants_and_lines(void) {
    const MWGraphWindow window = mw_graph_default_window();
    const MWGraphSamples constant = sample("2", window, 128, 43, 0, false);
    assert(constant.width == 128 && constant.height == 43);
    for(uint16_t i = 0; i < constant.width; ++i) {
        assert(constant.valid[i]);
        assert(constant.values[i] == 2);
        assert(constant.y[i] == 17);
        assert(constant.connect[i] == (i > 0));
    }
    const MWGraphSamples offscreen = sample("20", window, 128, 43, 0, false);
    for(uint16_t i = 0; i < offscreen.width; ++i) {
        assert(!offscreen.valid[i] && !offscreen.connect[i]);
        assert(offscreen.values[i] == 20);
    }
    const MWGraphSamples line = sample("x", window, 128, 43, 0, false);
    for(uint16_t i = 0; i < line.width; ++i) {
        assert(line.valid[i]);
        assert(line.connect[i] == (i > 0));
    }
    assert(line.y[0] == 42 && line.y[127] == 0);
    const MWGraphSamples ans = sample("ans", window, 2, 2, 10, false);
    assert(ans.valid[0] && ans.valid[1] && ans.connect[1]);
    assert(ans.y[0] == 0 && ans.values[0] == 10);
}

static void test_curves_and_domains(void) {
    const MWGraphWindow window = mw_graph_default_window();
    const MWGraphSamples square = sample("x^2", window, 127, 43, 0, false);
    assert(!square.valid[0] && !square.valid[126]);
    assert(square.valid[63] && square.y[63] == 21);
    assert(square.connect[63] && square.connect[64]);
    for(uint16_t i = 0; i < square.width; ++i) {
        assert(square.valid[i] == (square.values[i] <= 10));
        if(square.connect[i]) assert(i && square.valid[i - 1] && square.valid[i]);
    }
    const MWGraphSamples sine = sample("sin(x)", window, 128, 43, 0, false);
    for(uint16_t i = 0; i < sine.width; ++i) {
        assert(sine.valid[i] && sine.connect[i] == (i > 0));
        assert(fabsf(sine.values[i]) <= 1.0f);
    }
    const MWGraphWindow degrees_window = {-180, 180, -2, 2};
    const MWGraphSamples degrees = sample("sin(x)", degrees_window, 5, 43, 0, true);
    assert(fabsf(degrees.values[1] + 1) < 0.0001f);
    assert(fabsf(degrees.values[3] - 1) < 0.0001f);
    const MWGraphSamples root = sample("sqrt(x)", window, 127, 43, 0, false);
    for(uint16_t i = 0; i < 63; ++i) {
        assert(!root.valid[i] && !root.connect[i] && isnan(root.values[i]));
    }
    assert(root.valid[63] && root.values[63] == 0 && !root.connect[63]);
    assert(root.valid[126]);
}

static void test_discontinuities(void) {
    const MWGraphWindow window = mw_graph_default_window();
    const MWGraphSamples reciprocal = sample("1/x", window, 127, 43, 0, false);
    assert(!reciprocal.valid[63] && !reciprocal.connect[63]);
    assert(isnan(reciprocal.values[63]));
    assert(!reciprocal.connect[64]);

    /* Neither endpoint nor the midpoint is the pole, and both endpoints are
     * visible. An off-center singularity must not become a vertical line. */
    const MWGraphWindow shifted = {-0.21f, 0.19f, -1000, 1000};
    const MWGraphSamples between = sample("1/x", shifted, 8, 43, 0, false);
    bool found_crossing = false;
    for(uint16_t i = 1; i < between.width; ++i) {
        if(mw_graph_x(&shifted, i - 1, 8) < 0 && mw_graph_x(&shifted, i, 8) > 0) {
            found_crossing = true;
            assert(between.valid[i - 1] && between.valid[i]);
            assert(!between.connect[i]);
        }
    }
    assert(found_crossing);

    const MWGraphWindow tangent_window = {1.3f, 1.8f, -1000, 1000};
    const MWGraphSamples tangent = sample("tan(x)", tangent_window, 16, 43, 0, false);
    found_crossing = false;
    for(uint16_t i = 1; i < tangent.width; ++i) {
        if(tangent.values[i - 1] > 0 && tangent.values[i] < 0) {
            found_crossing = true;
            assert(!tangent.connect[i]);
        }
    }
    assert(found_crossing);
}

static void test_extremes_and_invalid_arguments(void) {
    MWGraphWindow window = mw_graph_default_window();
    const MWGraphSamples overflow = sample("10^100", window, 128, 43, 0, false);
    for(uint16_t i = 0; i < overflow.width; ++i) {
        assert(!overflow.valid[i] && !overflow.connect[i] && isnan(overflow.values[i]));
    }
    const MWGraphSamples huge = sample("1e30", window, 128, 43, 0, false);
    for(uint16_t i = 0; i < huge.width; ++i) {
        assert(!huge.valid[i] && !huge.connect[i] && isfinite(huge.values[i]));
    }
    MWProgram program;
    MWGraphSamples output;
    assert(mw_compile("x", &program, NULL) == MW_OK);
    memset(&output, 0x7f, sizeof(output));
    assert(!mw_graph_sample(&program, &window, 129, 43, 0, false, &output));
    assert(output.width == 0 && !output.valid[0] && !output.connect[127]);
    assert(!mw_graph_sample(&program, &window, 128, 0, 0, false, &output));
    assert(!mw_graph_sample(&program, &window, 128, 129, 0, false, &output));
    assert(!mw_graph_sample(&program, &window, 128, 43, NAN, false, &output));
    assert(!mw_graph_sample(&program, &window, 128, 43, 0, false, NULL));
    assert(!mw_graph_sample(NULL, &window, 128, 43, 0, false, &output));
    window.x_min = NAN;
    assert(!mw_graph_sample(&program, &window, 128, 43, 0, false, &output));
}

static void test_pan_zoom_and_bounds(void) {
    MWGraphWindow window = mw_graph_default_window();
    assert(mw_graph_pan(&window, 0.25f, -0.5f));
    assert(window.x_min == -5 && window.x_max == 15);
    assert(window.y_min == -20 && window.y_max == 0);
    assert(mw_graph_zoom(&window, 0.5f));
    assert(window.x_min == 0 && window.x_max == 10);
    assert(window.y_min == -15 && window.y_max == -5);
    assert(mw_graph_zoom(&window, 2));
    const MWGraphWindow saved = window;
    assert(!mw_graph_zoom(&window, 0));
    assert(!mw_graph_zoom(&window, -1));
    assert(!mw_graph_zoom(&window, NAN));
    assert(!mw_graph_zoom(&window, 1e30f));
    assert(!mw_graph_pan(&window, INFINITY, 0));
    assert(!mw_graph_pan(&window, 0, NAN));
    assert(!mw_graph_pan(&window, 1e30f, 0));
    assert(memcmp(&window, &saved, sizeof(window)) == 0);
    window = mw_graph_default_window();
    unsigned zoom_count = 0;
    while(mw_graph_zoom(&window, 0.5f)) {
        assert(mw_graph_window_valid(&window));
        assert(++zoom_count < 100);
    }
    assert(zoom_count > 10);
    assert(window.x_max - window.x_min >= MW_GRAPH_MIN_SPAN);
    window = mw_graph_default_window();
    zoom_count = 0;
    while(mw_graph_zoom(&window, 2)) {
        assert(mw_graph_window_valid(&window));
        assert(++zoom_count < 100);
    }
    assert(zoom_count > 10 && window.x_max <= MW_GRAPH_COORDINATE_LIMIT);
    assert(!mw_graph_window_valid(NULL));
    assert(!mw_graph_window_valid(&(MWGraphWindow){0, 0, -1, 1}));
    assert(!mw_graph_window_valid(&(MWGraphWindow){1, -1, -1, 1}));
    assert(!mw_graph_window_valid(&(MWGraphWindow){-1, INFINITY, -1, 1}));
    assert(!mw_graph_window_valid(&(MWGraphWindow){-1e7f, 1e7f, -1, 1}));
    assert(!mw_graph_pan(NULL, 1, 1));
    assert(!mw_graph_zoom(NULL, 0.5f));
}

int main(void) {
    test_coordinate_mapping();
    test_constants_and_lines();
    test_curves_and_domains();
    test_discontinuities();
    test_extremes_and_invalid_arguments();
    test_pan_zoom_and_bounds();
    puts("Graph tests passed (mapping, curves, poles, domains, bounds).");
    return 0;
}
