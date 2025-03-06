#include "app.h"
#include "src/alloc_free.h"
#include "libs/jsmn.h"
#include <storage/storage.h>

const uint16_t beacon_period_values[4] = {20, 50, 75, 100};
const char* beacon_period_names[4] = {"20ms", "50ms", "75ms", "100ms"};
const uint16_t beacon_duration_values[4] = {1000, 2000, 5000, 10000};
const char* beacon_duration_names[4] = {"1s", "2s", "5s", "10s"};
const char* randomize_mac_names[2] = {"Off", "On"};
static const char DEVICE_NAME_KEY[] = "device_name";
static const char BEACON_PERIOD_KEY[] = "bt_period_idx";
static const char BEACON_DURATION_KEY[] = "bt_duration_idx";
static const char RANDOMIZE_MAC_KEY[] = "bt_randomize_mac";

/**
 * @brief      Save path, ssid and password to file on change.
 * @param      app  The context
*/
void save_settings(App* app) {
    if(furi_mutex_acquire(app->config_mutex, FuriWaitForever) == FuriStatusOk) {
        FURI_LOG_I(TAG, "Saving json config...");
        BtBeacon* bt_model = view_get_model(app->view_bt);

        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* file = storage_file_alloc(storage);

        FuriJson* json = furi_json_alloc();

        furi_json_add_entry(json, DEVICE_NAME_KEY, bt_model->device_name);

        if(strlen(bt_model->device_name) == 0) {
            FURI_LOG_I(TAG, "%s", bt_model->default_device_name);
            char* p = memccpy(
                bt_model->device_name,
                bt_model->default_device_name,
                '\0',
                bt_model->device_name_len);
            if(!p) {
                bt_model->device_name[bt_model->device_name_len - 1] = '\0';
                FURI_LOG_I(
                    TAG,
                    "[save_settings]: Manually terminating string in [bt_model->device_name], check sizes");
            }
            variable_item_set_current_value_text(app->device_name_item, bt_model->device_name);
        }
        furi_json_add_entry(json, BEACON_PERIOD_KEY, (uint32_t)bt_model->beacon_period_idx);
        furi_json_add_entry(json, BEACON_DURATION_KEY, (uint32_t)bt_model->beacon_duration_idx);
        furi_json_add_entry(json, RANDOMIZE_MAC_KEY, (uint32_t)bt_model->randomize_mac_enb);

        size_t len_w = 0;
        size_t len_req = strlen(json->to_text);
        if(storage_file_open(file, BT_CONF_PATH, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
            storage_file_truncate(file);
            len_w = storage_file_write(file, json->to_text, len_req);
        } else {
            FURI_LOG_E(TAG, "Error opening %s for writing", BT_CONF_PATH);
        }
        storage_file_close(file);

        storage_file_free(file);
        furi_json_free(json);
        furi_record_close(RECORD_STORAGE);
        FURI_LOG_I(TAG, "Saving data completed, written %u bytes, buffer was %u", len_w, len_req);
        furi_check(furi_mutex_release(app->config_mutex) == FuriStatusOk);
    }
}

/**
 * @brief      Load path, ssid and password from file on start if available, otherwise set some default values.
 * @param      app  The context
*/
void load_settings(App* app) {
    FURI_LOG_I(TAG, "Loading json config...");
    BtBeacon* bt_model = view_get_model(app->view_bt);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_dir_exists(storage, BT_SETTINGS_FOLDER)) {
        FURI_LOG_I(TAG, "Folder missing, creating %s", BT_SETTINGS_FOLDER);
        storage_simply_mkdir(storage, BT_SETTINGS_FOLDER);
    }
    File* file = storage_file_alloc(storage);
    size_t buf_size = 512;
    uint8_t* file_buffer = malloc(buf_size);
    FuriString* json = furi_string_alloc();
    uint16_t max_tokens = 128;

    if(storage_file_open(file, BT_CONF_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, file_buffer, buf_size);
    } else {
        FURI_LOG_E(TAG, "Failed to open config file %s", BT_CONF_PATH);
    }
    storage_file_close(file);

    for(size_t i = 0; i < buf_size; i++) {
        furi_string_push_back(json, file_buffer[i]);
    }

    char* value;
    value = get_json_value(DEVICE_NAME_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        if(strlen(value) > 0) {
            char* p = memccpy(bt_model->device_name, value, '\0', bt_model->device_name_len);
            if(!p) {
                bt_model->device_name[bt_model->device_name_len - 1] = '\0';
                FURI_LOG_I(
                    TAG,
                    "[load_settings]: Manually terminating string in [bt_model->device_name], check sizes");
            }
        } else {
            if(strlen(bt_model->device_name) == 0) {
                char* p = memccpy(
                    bt_model->device_name,
                    bt_model->default_device_name,
                    '\0',
                    bt_model->device_name_len);
                if(!p) {
                    bt_model->device_name[bt_model->device_name_len - 1] = '\0';
                    FURI_LOG_I(
                        TAG,
                        "[load_settings]: Manually terminating string in [bt_model->device_name], check sizes");
                }
                variable_item_set_current_value_text(app->device_name_item, bt_model->device_name);
            }
        }
        free(value);
    } else {
        FURI_LOG_I(TAG, "Error: Key [%s] not found while loading config.", DEVICE_NAME_KEY);
    }
    value = get_json_value(BEACON_PERIOD_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        bt_model->beacon_period_idx = strtoul(value, NULL, 10);
        bt_model->beacon_period = beacon_period_values[bt_model->beacon_period_idx];
        free(value);
    } else {
        FURI_LOG_I(
            TAG,
            "Error: Key [%s] not found while loading config, using default value (%u).",
            BEACON_PERIOD_KEY,
            DEFAULT_BEACON_PERIOD);
        bt_model->beacon_period = DEFAULT_BEACON_PERIOD;
    }
    value = get_json_value(BEACON_DURATION_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        bt_model->beacon_duration_idx = strtoul(value, NULL, 10);
        bt_model->beacon_duration = beacon_duration_values[bt_model->beacon_duration_idx];
        free(value);
    } else {
        FURI_LOG_I(
            TAG,
            "Error: Key [%s] not found while loading config, using default value (%u).",
            BEACON_DURATION_KEY,
            DEFAULT_BEACON_DURATION);
        bt_model->beacon_duration = DEFAULT_BEACON_DURATION;
    }
    value = get_json_value(RANDOMIZE_MAC_KEY, furi_string_get_cstr(json), max_tokens);
    if(value) {
        uint32_t index = strtoul(value, NULL, 10);
        switch(index) {
        case 0:
            bt_model->randomize_mac_enb = false;
            break;
        case 1:
            bt_model->randomize_mac_enb = true;
            break;
        default:
            break;
        }
        free(value);
    } else {
        FURI_LOG_E(TAG, "Error: Key [%s] not found while loading config.", RANDOMIZE_MAC_KEY);
    }

    free(file_buffer);
    furi_string_free(json);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "Loading data completed");
}

/**
 * @brief      Callback for exiting the application.
 * @details    This function is called when user press back button.  We return VIEW_NONE to
 *             indicate that we want to exit the application.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_exit_callback(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

/**
 * @brief      Callback for returning to submenu.
 * @details    This function is called when user press back button.  We return ViewSubmenu to
 *             indicate that we want to navigate to the submenu.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_submenu_callback(void* context) {
    UNUSED(context);
    return ViewSubmenu;
}

/**
 * @brief      Callback for returning to configure screen.
 * @details    This function is called when user press back button.  We return ViewConfigure to
 *             indicate that we want to navigate to the configure screen.
 * @param      context  The context - unused
 * @return     next view id
*/
uint32_t navigation_configure_callback(void* context) {
    UNUSED(context);
    return ViewConfigure;
}

/**
 * @brief      Handle submenu item selection.
 * @details    This function is called when user selects an item from the submenu.
 * @param      context  The context - App object.
 * @param      index     The SubmenuIndex item that was clicked.
*/
void submenu_callback(void* context, uint32_t index) {
    App* app = (App*)context;
    switch(index) {
    case SubmenuIndexConfigure:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewConfigure);
        break;
    case SubmenuIndexBT:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewBt);
        break;
    case SubmenuIndexAbout:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewAbout);
        break;
    default:
        break;
    }
}

/**
 * @brief      Function called when one of the VariableItem is updated.
 * @param      item  The pointer to the VariableItem object.
*/
void variable_item_setting_changed(VariableItem* item) {
    App* app = variable_item_get_context(item);
    BtBeacon* bt_model = view_get_model(app->view_bt);

    uint8_t index = variable_item_list_get_selected_item_index(app->variable_item_list_config);
    FURI_LOG_I(TAG, "Index %u", index);
    switch(index) {
    case ConfigVariableItemBeaconPeriod:
        bt_model->beacon_period_idx = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(
            item, beacon_period_names[bt_model->beacon_period_idx]);
        bt_model->beacon_period = beacon_period_values[bt_model->beacon_period_idx];
        break;
    case ConfigVariableItemBeaconDuration:
        bt_model->beacon_duration_idx = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(
            item, beacon_duration_names[bt_model->beacon_duration_idx]);
        bt_model->beacon_duration = beacon_duration_values[bt_model->beacon_duration_idx];
        break;
    case ConfigVariableItemRandomizeMac:
        bt_model->randomize_mac_enb = variable_item_get_current_value_index(item);
        variable_item_set_current_value_text(
            item, randomize_mac_names[variable_item_get_current_value_index(item)]);
        break;

    default:
        FURI_LOG_E(TAG, "Unhandled index [%u] in variable_item_setting_changed.", index);
        return;
    }
    save_settings(app);
}

/**
 * @brief      Function called when one of the texts is updated.
 * @param      context  The context - App object.
*/
void conf_text_updated(void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);

    switch(app->config_index) {
    // Frame
    case ConfigTextInputDeviceName:
        char* p = memccpy(
            bt_model->device_name, app->temp_device_name, '\0', app->temp_device_name_size);
        if(!p) {
            bt_model->device_name[app->temp_device_name_size] = '\0';
            FURI_LOG_I(
                TAG,
                "[conf_text_updated]: Manually terminating string in [bt_model->device_name], check sizes");
        }
        variable_item_set_current_value_text(app->device_name_item, bt_model->device_name);
        break;
    default:
        FURI_LOG_E(TAG, "Unhandled index [%lu] in conf_text_updated.", app->config_index);
        return;
    }

    save_settings(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewConfigure);
}

/**
 * @brief      Callback when item in configuration screen is clicked.
 * @details    This function is called when user clicks OK on an item in the configuration screen.
 *             If the item clicked is a text field then we switch to the text input screen.
 * @param      context  The context - App object.
 * @param      index - The index of the item that was clicked.
*/
void setting_item_clicked(void* context, uint32_t index) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    size_t size;
    char* string;
    TextInput* text_input;
    uint8_t view_index;

    switch(index) {
    // Frame
    case ConfigTextInputDeviceName:
        app->temp_buffer = app->temp_device_name;
        size = app->temp_device_name_size;
        string = bt_model->device_name;
        text_input = app->text_input_device_name;
        text_input_set_minimum_length(text_input, 0);
        text_input_set_header_text(text_input, "Device Name:"),
            view_index = ViewTextInputDeviceName;
        break;
    default:
        // don't handle presses that are not explicitly defined in the enum
        return;
    }
    // Initialize temp_buffer with existing string
    char* p = memccpy(app->temp_buffer, string, '\0', size);
    if(!p) {
        app->temp_buffer[size] = '\0';
        FURI_LOG_I(
            TAG,
            "[settings_item_clicked]: Manually terminating string in [temp_buffer], check sizes");
    }
    // Configure the text input
    text_input_set_result_callback(
        text_input, conf_text_updated, app, app->temp_buffer, size, false);
    // Set the previous callback to return to Configure screen
    view_set_previous_callback(text_input_get_view(text_input), navigation_configure_callback);
    app->config_index = index;
    // Show text input dialog.
    view_dispatcher_switch_to_view(app->view_dispatcher, view_index);
}

/**
 * @brief      Callback to fire data update.
 * @details    This function is called when the timer ticks.
 * @param      context  The context - App object.
*/
void comm_thread_timer_callback(void* context) {
    App* app = (App*)context;
    furi_thread_flags_set(app->comm_thread_id, ThreadCommUpdData);
}

/**
 * @brief      Callback for custom events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - EventId value.
 * @param      context  The context - App object.
*/
bool view_custom_event_callback(uint32_t event, void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);

    switch(event) {
    // Redraw the screen, called by the timer callback every timer tick
    case EventIdBtRedrawScreen: {
        with_view_model(app->view_bt, BtBeacon * model, { UNUSED(model); }, true);
        return true;
    }
    case EventIdBtCheckBack:
        if(bt_model->status != BEACON_BUSY) {
            view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        }
        return true;
    case EventIdForceBack:
        view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Callback of the timer_reset to update the btton pressed graphics.
 * @details    This function is called when the timer_reset ticks.
 * @param      context  The context - App object.
*/
void view_timer_key_reset_callback(void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    bt_model->last_input = INPUT_RESET;
}

/**
 * @brief      Main function for  application.
 * @details    This function is the entry point for the  application.  It should be defined in
 *           application.fam as the entry_point setting.
 * @param      _p  Input parameter - unused
 * @return     0 - Success
*/
int32_t bt_home_remote_app(void* _p) {
    UNUSED(_p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
