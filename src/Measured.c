#include <pebble.h>
#include <Measured.h>
#define DEBUGLOG 0
#define CONFIG_VERSION "0.1"

// define the persistent storage key(s)
#define PK_SETTINGS      0
// define the appkeys used for appMessages
#define AK_STYLE_INV     0
#define AK_MESSAGE_TYPE          99
#define AK_TIMEZONE_OFFSET      103
#define AK_SEND_WATCH_VERSION   104
#define AK_SEND_CONFIG_VERSION  105

static Window *window;
static TextLayer *text_layer;
static InverterLayer *inverter_layer;
AppTimer *timezone_request = NULL;
// connected info
static bool bluetooth_connected = false;

#define TIMEZONE_UNINITIALIZED 80
static int8_t timezone_offset = TIMEZONE_UNINITIALIZED;
struct tm *currentTime;

persist settings = {
  .version    = 1,
  .inverted   = 0, // no, dark
};

persist_debug debug = {
  .general = false,     // debugging disabled by default
};

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Select");
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Up");
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  text_layer_set_text(text_layer, "Down");
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

static void request_timezone(void *data) {
  DictionaryIterator *iter;
  AppMessageResult result = app_message_outbox_begin(&iter);
  if (iter == NULL) {
    if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "iterator is null: %d", result); }
    return;
  }
  if (dict_write_uint8(iter, AK_MESSAGE_TYPE, AK_TIMEZONE_OFFSET) != DICT_OK) {
    return;
  }
  app_message_outbox_send();
  timezone_request = NULL;
}

void update_connection() {
  if (bluetooth_connected) {
    // determine which functions need to be disabled (e.g. GPS, sending)
  } else {
    // re-enable all functions
  }
}

static void handle_bluetooth(bool connected) {
  if (bluetooth_connected != connected) {
    bluetooth_connected = connected;
    update_connection();
    if (bluetooth_connected == true) {
      if ( (timezone_request == NULL) & (timezone_offset == TIMEZONE_UNINITIALIZED) ) {
        timezone_request = app_timer_register(5000, &request_timezone, NULL); // give it time to settle...
        if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "timezone request timer queued"); }
      }
    }
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  text_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(text_layer, "Press a button");
  text_layer_set_text_alignment(text_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(text_layer));

  // topmost inverter layer, determines dark or light...
  inverter_layer = inverter_layer_create(bounds);
  if (settings.inverted==0) {
    layer_set_hidden(inverter_layer_get_layer(inverter_layer), true);
  }
  layer_add_child(window_layer, inverter_layer_get_layer(inverter_layer));

}

static void window_unload(Window *window) {
  layer_destroy(inverter_layer_get_layer(inverter_layer));
  text_layer_destroy(text_layer);
}


static void watch_version_send(void *data) {
  DictionaryIterator *iter;

  AppMessageResult result = app_message_outbox_begin(&iter);

  if (iter == NULL) {
    if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "iterator is null: %d", result); }
    return;
  }

  if (result != APP_MSG_OK) {
    if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Dict write failed to open outbox: %d", (AppMessageResult) result); }
    return;
  }

  if (dict_write_uint8(iter, AK_MESSAGE_TYPE, AK_SEND_WATCH_VERSION) != DICT_OK) {
    return;
  }
  if (dict_write_uint8(iter, AK_SEND_WATCH_VERSION, settings.version) != DICT_OK) {
    return;
  }
  if (dict_write_cstring(iter, AK_SEND_CONFIG_VERSION, CONFIG_VERSION) != DICT_OK) {
    return;
  }
  app_message_outbox_send();
}

void my_out_sent_handler(DictionaryIterator *sent, void *context) {
// outgoing message was delivered
  if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "AppMessage Delivered"); }
}

void my_out_fail_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
// outgoing message failed
  if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "AppMessage Failed to Send: %d", reason); }
}

void in_js_ready_handler(DictionaryIterator *received, void *context) {
    watch_version_send(NULL);
}

void in_timezone_handler(DictionaryIterator *received, void *context) {
    Tuple *tz_offset = dict_find(received, AK_TIMEZONE_OFFSET);
    if (tz_offset != NULL) {
      timezone_offset = tz_offset->value->int8;
      // TODO - update any representation of the timezone we have
    }
  if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Timezone received: %d", timezone_offset); }
}

void in_configuration_handler(DictionaryIterator *received, void *context) {

    // style_inv == inverted
    Tuple *style_inv = dict_find(received, AK_STYLE_INV);
    if (style_inv != NULL) {
      settings.inverted = style_inv->value->uint8;
      if (style_inv->value->uint8==0) {
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), true); // hide inversion = dark
      } else {
        layer_set_hidden(inverter_layer_get_layer(inverter_layer), false); // show inversion = light
      }
    }

    int result = 0;
    result = persist_write_data(PK_SETTINGS, &settings, sizeof(settings) );
    if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Wrote %d bytes into settings", result); }

    // TODO - redraw!
}

void my_in_rcv_handler(DictionaryIterator *received, void *context) {
// incoming message received
  Tuple *message_type = dict_find(received, AK_MESSAGE_TYPE);
  if (message_type != NULL) {
    if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "Message type %d received", message_type->value->uint8); }
    switch ( message_type->value->uint8 ) {
    case AK_SEND_WATCH_VERSION:
      in_js_ready_handler(received, context);
      return;
    case AK_TIMEZONE_OFFSET:
      in_timezone_handler(received, context);
      return;
    }
  } else {
    // default to configuration, which may not send the message type...
    in_configuration_handler(received, context);
  }
}

void my_in_drp_handler(AppMessageResult reason, void *context) {
// incoming message dropped
  if (debug.general) { app_log(APP_LOG_LEVEL_DEBUG, __FILE__, __LINE__, "AppMessage Dropped: %d", reason); }
}

struct tm *get_time() {
    time_t tt = time(0);
    return localtime(&tt);
}

static void app_message_init(void) {
  // Register message handlers
  app_message_register_inbox_received(my_in_rcv_handler);
  app_message_register_inbox_dropped(my_in_drp_handler);
  app_message_register_outbox_sent(my_out_sent_handler);
  app_message_register_outbox_failed(my_out_fail_handler);
  // Init buffers
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void init(void) {
  currentTime = get_time();

  app_message_init();
  if (persist_exists(PK_SETTINGS)) {
    persist_read_data(PK_SETTINGS, &settings, sizeof(settings) );
  }

  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_set_background_color(window, GColorBlack);
  window_stack_push(window, false);
  bluetooth_connection_service_subscribe(&handle_bluetooth);
  handle_bluetooth(bluetooth_connection_service_peek()); // initialize

}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
