#include "utils.h"
#include "libs/jsmn.h"
#include <storage/storage.h>

static const char DEVICE_NAME_KEY[] = "device_name";
static const char BEACON_PERIOD_KEY[] = "bt_period_idx";
static const char BEACON_DURATION_KEY[] = "bt_duration_idx";
static const char RANDOMIZE_MAC_KEY[] = "bt_randomize_mac";

extern const uint16_t beacon_period_values[4];
extern const char* beacon_period_names[4];
extern uint16_t beacon_duration_values[4];
extern char* beacon_duration_names[4];

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
 * @brief      Short buzz the vibration
*/
void buzz_vibration(uint32_t ms) {
    furi_hal_vibro_on(true);
    furi_delay_ms(ms);
    furi_hal_vibro_on(false);
}

/**
 * @brief      Generate a random number bewteen min and max
 * @param      min  minimum value
 * @param      max  maximum value
 * @return     the random number
*/
int32_t random_limit(int32_t min, int32_t max) {
    int32_t rnd = rand() % (max + 1 - min) + min;
    return rnd;
}

/**
 * @brief      Reverse an uint8_t array
 * @param      arr  pointer to the array to be reversed
 * @param      size  the array size
*/
void reverse_array_uint8(uint8_t* arr, size_t size) {
    uint8_t temp[size];

    for(size_t i = 0; i < size; i++)
        temp[i] = arr[size - i - 1];

    for(size_t i = 0; i < size; i++)
        arr[i] = temp[i];
}

VariableItem* variable_item_init(
    VariableItemList* item_list,
    const char* label,
    const char* curr_value_text,
    uint8_t values_count,
    uint8_t curr_idx,
    VariableItemChangeCallback callback,
    void* context) {
    VariableItem* item;

    if(callback && context) {
        item = variable_item_list_add(item_list, label, values_count, callback, context);
    } else {
        item = variable_item_list_add(item_list, label, values_count, NULL, NULL);
    }
    if(curr_idx > 1) {
        variable_item_set_current_value_index(item, curr_idx);
    }
    variable_item_set_current_value_text(item, curr_value_text);
    return item;
}

void draw_header(Canvas* canvas, const char* title, const int8_t curr_page, const int32_t y_pos) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 0, y_pos, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, y_pos + 1, 126, y_pos + 1);
    char page_num[6];
    snprintf(page_num, sizeof(page_num), "%i", curr_page);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 116, y_pos, page_num);
    canvas_set_font(canvas, FontSecondary);
}
