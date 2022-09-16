#include <furi.h>
#include <dialogs/dialogs.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} PluginEvent;

typedef struct {
    int taps;
    float interval;
    float bpm;
} BPMTapper;

static void show_hello() {

  // BEGIN HELLO DIALOG
  DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
  DialogMessage* message = dialog_message_alloc();

  const char* header_text = "BPM Tapper";
  const char* message_text = "Tap center to start";

  dialog_message_set_header(message, header_text, 63, 3, AlignCenter, AlignTop);
  dialog_message_set_text(message, message_text, 0, 17, AlignLeft, AlignTop);
  dialog_message_set_buttons(message, NULL, "Tap", NULL);

  dialog_message_set_icon(message, &I_DolphinCommon_56x48, 72, 17);

  dialog_message_show(dialogs, message);

  dialog_message_free(message);
  furi_record_close(RECORD_DIALOGS);
  // END HELLO DIALOG
}

static void input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue); 

    PluginEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void render_callback(Canvas* const canvas, void* ctx) {
    const BPMTapper* bpm_state = acquire_mutex((ValueMutex*)ctx, 25);
    if (bpm_state == NULL) {
      return;
    }
    // border
    canvas_draw_frame(canvas, 0, 0, 128, 64);
    canvas_set_font(canvas, FontPrimary);
    char taps[8];
    //sprintf(taps, "%d", bpm_state->taps);
    itoa(bpm_state->taps, taps, 10);
    canvas_draw_str_aligned(canvas, 5, 5, AlignRight, AlignBottom, taps);
    release_mutex((ValueMutex*)ctx, bpm_state);
}

static void bpm_state_init(BPMTapper* const plugin_state) {
  plugin_state->taps = 0; 
  plugin_state->bpm = 0.0;
}

int32_t bpm_tapper_app(void* p) {
  UNUSED(p);

  FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(PluginEvent));
  
  BPMTapper* bpm_state = malloc(sizeof(BPMTapper));
  // setup
  bpm_state_init(bpm_state);
  
  ValueMutex state_mutex;
  if (!init_mutex(&state_mutex, bpm_state, sizeof(bpm_state))) {
      FURI_LOG_E("BPM-Tapper", "cannot create mutex\r\n");
      free(bpm_state);
      return 255;
  }
  show_hello();

  // BEGIN IMPLEMENTATION

  // Set system callbacks
  ViewPort* view_port = view_port_alloc(); 
  view_port_draw_callback_set(view_port, render_callback, &state_mutex);
  view_port_input_callback_set(view_port, input_callback, event_queue);
  
  // Open GUI and register view_port
  Gui* gui = furi_record_open("gui"); 
  gui_add_view_port(gui, view_port, GuiLayerFullscreen);

  PluginEvent event;
  for (bool processing = true; processing;) {
    FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);
    BPMTapper* bpm_state = (BPMTapper*)acquire_mutex_block(&state_mutex);
    if(event_status == FuriStatusOk) {
      // press events
      if(event.type == EventTypeKey) {
        if(event.input.type == InputTypePress) {  
          switch(event.input.key) {
            case InputKeyUp:
            case InputKeyDown:
            case InputKeyRight:
            case InputKeyLeft:
            case InputKeyOk:
              bpm_state->taps++;
              break;
            case InputKeyBack:
                // Exit the plugin
                processing = false;
                break;
          }
        }
      } 
    } else {
      FURI_LOG_D("BPM-Tapper", "FuriMessageQueue: event timeout");
    // event timeout
    }
    view_port_update(view_port);
    release_mutex(&state_mutex, bpm_state);
  }
  view_port_enabled_set(view_port, false);
  gui_remove_view_port(gui, view_port);
  furi_record_close("gui");
  view_port_free(view_port);
  furi_message_queue_free(event_queue);
  delete_mutex(&state_mutex);
  //free(bpm_state);

  return 0;
}
