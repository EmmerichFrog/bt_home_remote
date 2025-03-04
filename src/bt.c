#include "bt.h"
#include "app.h"
#include "bt_home_remote_icons.h"
#include "src/utils.h"

/**
 * @brief      Check if sending a request is allowed
 * @param      model  the current model
 * @return     true if it's allowed
*/
bool allow_cmd_bt(BtBeacon* bt_model) {
    return bt_model->status != BEACON_BUSY;
}

/**
 * @brief      Callback of the timer_reset to update the btton pressed graphics.
 * @details    This function is called when the timer_reset ticks.
 * @param      context  The context - App object.
*/
void timer_beacon_reset_callback(void* context) {
    App* app = (App*)context;
    furi_thread_flags_set(app->comm_thread_id, ThreadCommStopCmd);
}

bool make_packet(BtBeacon* bt_model, uint8_t* _size, uint8_t** _packet) {
    uint8_t* packet = malloc(EXTRA_BEACON_MAX_DATA_SIZE);
    size_t i = 0;
    bt_model->cnt++;

    // Flag data
    packet[i++] = 0x02; // length
    packet[i++] = 0x03; // Type: Flags
    packet[i++] =
        0b00000110; // bit 1: “LE General Discoverable Mode”, bit 2: “BR/EDR Not Supported”
    // Service data
    packet[i++] = 0x08; // length
    packet[i++] = 0x16; // Type: Flags
    // BTHome Data
    packet[i++] = 0xD2; // UUID 1
    packet[i++] = 0xFC; // UUID 2
    packet[i++] = 0b01000100; // BTHome Device Information
    // Actual Data
    packet[i++] = 0x3A; // Type: Object ID Button
    packet[i++] = 0x01; // Event Press
    // Packet Id
    packet[i++] = 0x00; // Type: Packet ID
    packet[i++] = bt_model->cnt; // Packet Counter
    //Device name
    packet[i++] = bt_model->device_name_len + 1; // Lenght
    packet[i++] = 0x09; // Full name

    for(size_t j = 0; j < bt_model->device_name_len; j++) {
        packet[i++] = (uint8_t)bt_model->device_name[j];
    }

    if(i > EXTRA_BEACON_MAX_DATA_SIZE) {
        FURI_LOG_E(BT_TAG, "Packet too big: Max = %u, Size = %u", EXTRA_BEACON_MAX_DATA_SIZE, i);
        free(packet);
        return false;
    }

    packet = realloc(packet, i);
    *_size = i;
    *_packet = packet;

    return true;
}

void randomize_mac(uint8_t address[EXTRA_BEACON_MAC_ADDR_SIZE]) {
    furi_hal_random_fill_buf(address, EXTRA_BEACON_MAC_ADDR_SIZE);
}

void pretty_print_mac(FuriString* mac_str, uint8_t address[EXTRA_BEACON_MAC_ADDR_SIZE]) {
    furi_string_set_str(mac_str, "");
    for(size_t i = 0; i < EXTRA_BEACON_MAC_ADDR_SIZE - 1; i++) {
        furi_string_cat_printf(mac_str, "%02X", address[i]);
        if(i < EXTRA_BEACON_MAC_ADDR_SIZE - 2) {
            furi_string_cat_str(mac_str, ":");
        }
    }
    FURI_LOG_I(BT_TAG, "Current MAC address: %s", furi_string_get_cstr(mac_str));
}
/**
 * @brief      Callback of the timer_draw to update the canvas.
 * @details    This function is called when the timer_draw ticks. Also update the data
 * @param      context  The context - App object.
*/
static void view_bt_timer_callback(void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    // If the mutex is not available the canvas is not finished drawing, so skip this timer tick.
    // Callback function cannot block so waiting is not allowed
    if(furi_mutex_acquire(bt_model->worker_mutex, 0) == FuriStatusOk) {
        // If mutex is available it's not needed here so release it
        furi_check(furi_mutex_release(bt_model->worker_mutex) == FuriStatusOk);
        view_dispatcher_send_custom_event(app->view_dispatcher, EventIdBtRedrawScreen);
    }
}

/**
 * @brief      Callback of the frame screen on enter.
 * @details    Prepare the timer_draw and reset get status.
 * @param      context  The context - App object.
*/
void bt_enter_callback(void* context) {
    App* app = (App*)context;
    app->current_view = ViewBt;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    // Beacon Setup
    FURI_LOG_I(BT_TAG, "%u, %u", bt_model->beacon_period, bt_model->beacon_duration);
    bt_model->config.min_adv_interval_ms = bt_model->beacon_period;
    bt_model->config.max_adv_interval_ms = bt_model->beacon_period * 1.5;

    const uint8_t fixed_mac[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    if(bt_model->randomize_mac_enb) {
        randomize_mac(bt_model->config.address);
    } else {
        memcpy(bt_model->config.address, fixed_mac, EXTRA_BEACON_MAC_ADDR_SIZE * sizeof(uint8_t));
    }

    pretty_print_mac(bt_model->mac_address_str, bt_model->config.address);
    // The beacon expects the MAC address in reverse order
    reverse_array_uint8(bt_model->config.address, EXTRA_BEACON_MAC_ADDR_SIZE);
    bt_model->timer_reset_beacon =
        furi_timer_alloc(timer_beacon_reset_callback, FuriTimerTypeOnce, context);
    // End Beacon
    app->timer_draw = furi_timer_alloc(view_bt_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(app->timer_draw, furi_ms_to_ticks(DRAW_PERIOD));

    app->timer_reset_key =
        furi_timer_alloc(view_timer_key_reset_callback, FuriTimerTypeOnce, context);

    app->comm_thread = furi_thread_alloc();
    furi_thread_set_name(app->comm_thread, "Comm_Thread");
    furi_thread_set_stack_size(app->comm_thread, 2048);
    furi_thread_set_context(app->comm_thread, app);
    furi_thread_set_callback(app->comm_thread, bt_comm_worker);
    furi_thread_start(app->comm_thread);
    app->comm_thread_id = furi_thread_get_id(app->comm_thread);
}

/**
 * @brief      Callback of the frame screen on exit.
 * @param      context  The context - App object.
*/
void bt_exit_callback(void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    furi_timer_flush();
    furi_timer_stop(app->timer_draw);
    furi_timer_free(app->timer_draw);
    app->timer_draw = NULL;
    furi_timer_stop(app->timer_reset_key);
    furi_timer_free(app->timer_reset_key);
    app->timer_reset_key = NULL;
    furi_timer_stop(bt_model->timer_reset_beacon);
    furi_timer_free(bt_model->timer_reset_beacon);
    bt_model->timer_reset_beacon = NULL;

    // Stop thread and wait for exit
    if(app->comm_thread) {
        furi_thread_flags_set(app->comm_thread_id, ThreadCommStop);
        furi_thread_join(app->comm_thread);
        furi_thread_free(app->comm_thread);
    }
}

/**
 * @brief      Callback for drawing the frame view.
 * @details    This function is called when the screen needs to be redrawn.
 * @param      canvas  The canvas to draw on.
 * @param      model   The model - MyModel object.
*/
void bt_draw_callback(Canvas* canvas, void* model) {
    BtBeacon* bt_model = (BtBeacon*)model;
    const uint8_t status = bt_model->status;
    const uint8_t packet_id = bt_model->cnt;
    FuriString* mac_address = furi_string_alloc();
    furi_string_set_str(mac_address, furi_string_get_cstr(bt_model->mac_address_str));

    if(furi_mutex_acquire(bt_model->worker_mutex, FuriWaitForever) == FuriStatusOk) {
        canvas_set_bitmap_mode(canvas, true);

        switch(bt_model->curr_page) {
        case PageFirst:
            draw_header(canvas, "Dehum. Switch", bt_model->curr_page, 8);
            canvas_draw_icon(canvas, 123, 2, &I_ButtonRightSmall_3x5);
            break;

        case PageSecond:
            draw_header(canvas, "MAC", bt_model->curr_page, 8);
            canvas_draw_icon(canvas, 111, 2, &I_ButtonLeftSmall_3x5);
            canvas_draw_str(canvas, 28, 8, furi_string_get_cstr(mac_address));
            break;

        case PageLast:

        default:
            break;
        }
        canvas_draw_str(canvas, 87, 60, "Cnt:");
        char cnt[6];
        snprintf(cnt, sizeof(cnt), "%u", packet_id);
        canvas_draw_str(canvas, 111, 60, cnt);

        canvas_draw_icon(canvas, 93, 19, &I_BLE_beacon_7x8);
        if(bt_model->last_input == InputKeyOk) {
            canvas_draw_icon(canvas, 87, 28, &I_ok_hover);
        } else {
            canvas_draw_icon(canvas, 87, 28, &I_ok);
        }

        switch(status) {
        case BEACON_INACTIVE:
            canvas_draw_icon(canvas, -1, 16, &I_DolphinCommon);
            break;
        case BEACON_BUSY:
            canvas_draw_icon(canvas, 0, 9, &I_NFC_dolphin_emulation_51x64);
            break;
        default:
            break;
        }

        furi_check(furi_mutex_release(bt_model->worker_mutex) == FuriStatusOk);
    }
    furi_string_free(mac_address);
}

/**
 * @brief      Callback for bt screen input.
 * @details    This function is called when the user presses a button while on the bt screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - App object.
 * @return     true if the event was handled, false otherwise.
*/
bool bt_input_callback(InputEvent* event, void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    if(event->key == InputKeyOk || event->key == InputKeyLeft || event->key == InputKeyRight ||
       event->key == InputKeyUp || event->key == InputKeyDown) {
        if(bt_model->last_input == event->key || bt_model->last_input == INPUT_RESET) {
            furi_timer_restart(app->timer_reset_key, furi_ms_to_ticks(RESET_KEY_PERIOD));
        } else {
            furi_timer_stop(app->timer_reset_key);
            bt_model->last_input = INPUT_RESET;
        }
    }
    // Status used for drawing button presses
    bt_model->last_input = event->key;
    int8_t p_index;
    // Ok sends the command selected with left, right or down. Up long press sends a shfhttp->utdown command
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyLeft:
            p_index = bt_model->curr_page - 1;
            if(p_index >= PageFirst) {
                bt_model->curr_page = p_index;
            }
            break;
        case InputKeyRight:
            p_index = bt_model->curr_page + 1;
            if(p_index <= PageLast) {
                bt_model->curr_page = p_index;
            }
            break;
        case InputKeyOk:
            if(allow_cmd_bt(bt_model)) {
                furi_thread_flags_set(app->comm_thread_id, ThreadCommSendCmd);
            }
            break;
        case InputKeyBack:
            view_dispatcher_send_custom_event(app->view_dispatcher, EventIdBtCheckBack);
            break;
        default:
            return false;
        }
        view_dispatcher_send_custom_event(app->view_dispatcher, EventIdBtRedrawScreen);
        return true;
    } else if(event->type == InputTypeLong) {
    }

    return false;
}

int32_t bt_comm_worker(void* context) {
    App* app = (App*)context;
    BtBeacon* bt_model = view_get_model(app->view_bt);
    bool run = true;

    while(run) {
        uint32_t events = furi_thread_flags_wait(
            ThreadCommStop | ThreadCommStopCmd | ThreadCommSendCmd,
            FuriFlagWaitAny,
            FuriWaitForever);
        if(events & ThreadCommStop) {
            run = false;
            FURI_LOG_I(TAG, "Thread event: Stop command request");

        } else if(events & ThreadCommStopCmd) {
            bt_model->status = BEACON_INACTIVE;
            FURI_LOG_I(BT_TAG, "Resetting Beacon...");
            if(furi_hal_bt_extra_beacon_is_active()) {
                furi_check(furi_hal_bt_extra_beacon_stop());
            }
            FURI_LOG_I(BT_TAG, "Resetting Beacon done.");
        } else if(events & ThreadCommSendCmd) {
            bt_model->status = BEACON_BUSY;
            FURI_LOG_I(BT_TAG, "Sending BTHome data...");
            if(furi_hal_bt_extra_beacon_is_active()) {
                furi_check(furi_hal_bt_extra_beacon_stop());
            }

            uint8_t size;
            uint8_t* packet;
            GapExtraBeaconConfig* config = &bt_model->config;

            furi_check(furi_hal_bt_extra_beacon_set_config(config));
            if(make_packet(bt_model, &size, &packet)) {
                furi_check(furi_hal_bt_extra_beacon_set_data(packet, size));
                furi_check(furi_hal_bt_extra_beacon_start());
                furi_timer_start(bt_model->timer_reset_beacon, bt_model->beacon_duration);
                free(packet);
            }
        }
    }
    FURI_LOG_I(TAG, "Thread event: Stopping...");
    return 0;
}
