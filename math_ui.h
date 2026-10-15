#pragma once

#include "math_app.h"
#include <gui/canvas.h>

/* Pure drawing: app state and its cached graph are never modified. */
void mw_draw(Canvas* canvas, const MWApp* app);
