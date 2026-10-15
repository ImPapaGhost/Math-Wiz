#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#include "math_app.h"
#include "math_ui.h"

typedef struct {
    MWApp model;
    FuriMutex* mutex;
    FuriMessageQueue* input;
} MathWiz;

static void draw(Canvas* canvas, void* context) {
    MathWiz* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    mw_draw(canvas, &app->model);
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* context) {
    MathWiz* app = context;
    /* Never block the GUI thread if inputs arrive faster than they are consumed. */
    furi_message_queue_put(app->input, event, 0);
}

static MWKey key_for(InputKey key) {
    switch(key) {
    case InputKeyUp: return MW_KEY_UP;
    case InputKeyDown: return MW_KEY_DOWN;
    case InputKeyLeft: return MW_KEY_LEFT;
    case InputKeyRight: return MW_KEY_RIGHT;
    case InputKeyOk: return MW_KEY_OK;
    default: return MW_KEY_BACK;
    }
}

int32_t math_wiz_main(void* context) {
    UNUSED(context);
    MathWiz* app = malloc(sizeof(MathWiz));
    furi_check(app);
    mw_app_init(&app->model);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* viewport = view_port_alloc();
    view_port_draw_callback_set(viewport, draw, app);
    view_port_input_callback_set(viewport, input_callback, app);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, viewport, GuiLayerFullscreen);

    while(app->model.running) {
        InputEvent event;
        if(furi_message_queue_get(app->input, &event, FuriWaitForever) != FuriStatusOk) continue;
        if(event.type != InputTypeShort && event.type != InputTypeLong && event.type != InputTypeRepeat) continue;
        MWInput input = event.type == InputTypeLong ? MW_INPUT_LONG :
            (event.type == InputTypeRepeat ? MW_INPUT_REPEAT : MW_INPUT_SHORT);
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        mw_app_handle(&app->model, key_for(event.key), input);
        furi_mutex_release(app->mutex);
        view_port_update(viewport);
    }

    view_port_enabled_set(viewport, false);
    gui_remove_view_port(gui, viewport);
    view_port_free(viewport);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->input);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
