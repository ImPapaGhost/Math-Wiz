#ifndef MATH_GRAPH_H
#define MATH_GRAPH_H

#include "math_engine.h"

#include <stdbool.h>
#include <stdint.h>

#define MW_GRAPH_MAX_WIDTH 128u
#define MW_GRAPH_MAX_HEIGHT 128u
#define MW_GRAPH_COORDINATE_LIMIT 1000000.0f
#define MW_GRAPH_MIN_SPAN 0.0001f

typedef struct {
    float x_min;
    float x_max;
    float y_min;
    float y_max;
} MWGraphWindow;

typedef struct {
    uint16_t width;
    uint16_t height;
    /* Values are retained even outside the window for a trace readout. A
     * failed evaluation produces NAN. valid means the pixel is visible. */
    float values[MW_GRAPH_MAX_WIDTH];
    int16_t y[MW_GRAPH_MAX_WIDTH];
    bool valid[MW_GRAPH_MAX_WIDTH];
    /* connect[i] permits a line from (i-1, y[i-1]) to (i, y[i]).
     * connect[0] is always false. Draw visible isolated points as well. */
    bool connect[MW_GRAPH_MAX_WIDTH];
} MWGraphSamples;

MWGraphWindow mw_graph_default_window(void);
bool mw_graph_window_valid(const MWGraphWindow* window);

/* These transforms preserve the original window if the result is invalid.
 * Pan distances are fractions of the current spans; positive y moves up.
 * Zoom is centered: a factor below one zooms in, above one zooms out. */
bool mw_graph_pan(MWGraphWindow* window, float x_fraction, float y_fraction);
bool mw_graph_zoom(MWGraphWindow* window, float factor);

/* Coordinates are local to the plot rectangle (no screen origin added).
 * Width/height must each be in [2,128]. An invalid x request returns NAN.
 * Project writes pixel outputs only when the point lies inside the window. */
float mw_graph_x(const MWGraphWindow* window, uint16_t column, uint16_t width);
bool mw_graph_project(
    const MWGraphWindow* window,
    float x,
    float y,
    uint16_t width,
    uint16_t height,
    int16_t* pixel_x,
    int16_t* pixel_y);

/* Evaluates each column and the midpoints between visible samples. Domain
 * failures and offscreen values leave gaps. Midpoint checks conservatively
 * reject suspicious joins across poles, but finite sampling cannot detect
 * every discontinuity or resolve oscillations narrower than a pixel.
 * Returns false for invalid arguments and clears output when provided. */
bool mw_graph_sample(
    const MWProgram* program,
    const MWGraphWindow* window,
    uint16_t width,
    uint16_t height,
    float ans,
    bool degrees,
    MWGraphSamples* output);

#endif
