#include "math_graph.h"

#include <math.h>
#include <string.h>

static bool dimensions_valid(uint16_t width, uint16_t height) {
    return width >= 2 && width <= MW_GRAPH_MAX_WIDTH && height >= 2 &&
           height <= MW_GRAPH_MAX_HEIGHT;
}

MWGraphWindow mw_graph_default_window(void) {
    return (MWGraphWindow){.x_min = -10, .x_max = 10, .y_min = -10, .y_max = 10};
}

bool mw_graph_window_valid(const MWGraphWindow* window) {
    if(!window || !isfinite(window->x_min) || !isfinite(window->x_max) ||
       !isfinite(window->y_min) || !isfinite(window->y_max)) {
        return false;
    }
    if(fabsf(window->x_min) > MW_GRAPH_COORDINATE_LIMIT ||
       fabsf(window->x_max) > MW_GRAPH_COORDINATE_LIMIT ||
       fabsf(window->y_min) > MW_GRAPH_COORDINATE_LIMIT ||
       fabsf(window->y_max) > MW_GRAPH_COORDINATE_LIMIT) {
        return false;
    }
    return window->x_max - window->x_min >= MW_GRAPH_MIN_SPAN &&
           window->y_max - window->y_min >= MW_GRAPH_MIN_SPAN;
}

bool mw_graph_pan(MWGraphWindow* window, float x_fraction, float y_fraction) {
    if(!mw_graph_window_valid(window) || !isfinite(x_fraction) || !isfinite(y_fraction)) {
        return false;
    }
    const float dx = (window->x_max - window->x_min) * x_fraction;
    const float dy = (window->y_max - window->y_min) * y_fraction;
    const MWGraphWindow next = {
        .x_min = window->x_min + dx,
        .x_max = window->x_max + dx,
        .y_min = window->y_min + dy,
        .y_max = window->y_max + dy,
    };
    if(!mw_graph_window_valid(&next)) return false;
    *window = next;
    return true;
}

bool mw_graph_zoom(MWGraphWindow* window, float factor) {
    if(!mw_graph_window_valid(window) || !isfinite(factor) || factor <= 0) return false;
    const float center_x = window->x_min + (window->x_max - window->x_min) * 0.5f;
    const float center_y = window->y_min + (window->y_max - window->y_min) * 0.5f;
    const float half_x = (window->x_max - window->x_min) * 0.5f * factor;
    const float half_y = (window->y_max - window->y_min) * 0.5f * factor;
    const MWGraphWindow next = {
        .x_min = center_x - half_x,
        .x_max = center_x + half_x,
        .y_min = center_y - half_y,
        .y_max = center_y + half_y,
    };
    if(!mw_graph_window_valid(&next)) return false;
    *window = next;
    return true;
}

float mw_graph_x(const MWGraphWindow* window, uint16_t column, uint16_t width) {
    if(!mw_graph_window_valid(window) || width < 2 || width > MW_GRAPH_MAX_WIDTH ||
       column >= width) {
        return NAN;
    }
    if(column == width - 1) return window->x_max;
    return window->x_min +
           (window->x_max - window->x_min) * ((float)column / (float)(width - 1));
}

static int16_t project_y(const MWGraphWindow* window, float y, uint16_t height) {
    const float pixel =
        (window->y_max - y) / (window->y_max - window->y_min) * (float)(height - 1);
    return (int16_t)(pixel + 0.5f);
}

bool mw_graph_project(
    const MWGraphWindow* window,
    float x,
    float y,
    uint16_t width,
    uint16_t height,
    int16_t* pixel_x,
    int16_t* pixel_y) {
    if(!mw_graph_window_valid(window) || !dimensions_valid(width, height) || !pixel_x ||
       !pixel_y || !isfinite(x) || !isfinite(y) || x < window->x_min || x > window->x_max ||
       y < window->y_min || y > window->y_max) {
        return false;
    }
    const float pixel =
        (x - window->x_min) / (window->x_max - window->x_min) * (float)(width - 1);
    *pixel_x = (int16_t)(pixel + 0.5f);
    *pixel_y = project_y(window, y, height);
    return true;
}

static bool may_connect(
    const MWProgram* program,
    const MWGraphWindow* window,
    float previous_x,
    float current_x,
    float previous_y,
    float current_y,
    uint16_t height,
    float ans,
    bool degrees) {
    float midpoint_y;
    const float midpoint_x = previous_x + (current_x - previous_x) * 0.5f;
    if(mw_evaluate(program, midpoint_x, ans, degrees, &midpoint_y) != MW_OK ||
       !isfinite(midpoint_y) || midpoint_y < window->y_min || midpoint_y > window->y_max) {
        return false;
    }

    /* A reciprocal or tan pole can sit between columns even when neither
     * endpoint overflows. Its midpoint reverses the apparent endpoint trend.
     * Test the sign crossing without multiplying potentially large values. */
    const bool crosses_zero = (previous_y < 0 && current_y > 0) ||
                              (previous_y > 0 && current_y < 0);
    const float low = previous_y < current_y ? previous_y : current_y;
    const float high = previous_y > current_y ? previous_y : current_y;
    if(crosses_zero && (midpoint_y < low || midpoint_y > high)) return false;

    /* More than two pixels of midpoint curvature is poorly represented by
     * a single straight segment; leave a gap instead of an invented line. */
    const float midpoint_linear = previous_y + (current_y - previous_y) * 0.5f;
    const float error_pixels = fabsf(midpoint_y - midpoint_linear) /
                               (window->y_max - window->y_min) * (float)(height - 1);
    return error_pixels <= 2.0f;
}

bool mw_graph_sample(
    const MWProgram* program,
    const MWGraphWindow* window,
    uint16_t width,
    uint16_t height,
    float ans,
    bool degrees,
    MWGraphSamples* output) {
    if(!output) return false;
    memset(output, 0, sizeof(*output));
    if(!program || !program->length || !mw_graph_window_valid(window) ||
       !dimensions_valid(width, height) || !isfinite(ans)) {
        return false;
    }
    output->width = width;
    output->height = height;
    float previous_x = window->x_min;
    for(uint16_t column = 0; column < width; ++column) {
        const float x = mw_graph_x(window, column, width);
        float y;
        output->values[column] = NAN;
        if(mw_evaluate(program, x, ans, degrees, &y) == MW_OK && isfinite(y)) {
            output->values[column] = y;
            if(y >= window->y_min && y <= window->y_max) {
                output->valid[column] = true;
                output->y[column] = project_y(window, y, height);
                if(column && output->valid[column - 1]) {
                    output->connect[column] = may_connect(
                        program,
                        window,
                        previous_x,
                        x,
                        output->values[column - 1],
                        y,
                        height,
                        ans,
                        degrees);
                }
            }
        }
        previous_x = x;
    }
    return true;
}
